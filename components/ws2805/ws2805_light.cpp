#include "ws2805_light.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ws2805 {

static const char *const TAG = "ws2805";
static const size_t RMT_SYMBOLS_PER_BYTE = 8;

uint32_t WS2805LightOutput::ws2805_rmt_resolution_hz() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  uint32_t freq;
  esp_clk_tree_src_get_freq_hz((soc_module_clk_t) RMT_CLK_SRC_DEFAULT, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq);
  return freq;
#else
  return 10000000; // 10MHz resolution (80MHz APB / 8 clk_div)
#endif
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
size_t IRAM_ATTR HOT WS2805LightOutput::ws2805_encoder_callback(const void *data, size_t size, size_t symbols_written, size_t symbols_free,
                                             rmt_symbol_word_t *symbols, bool *done, void *arg) {
  auto *params = static_cast<LedParams *>(arg);
  const auto *bytes = static_cast<const uint8_t *>(data);
  size_t index = symbols_written / RMT_SYMBOLS_PER_BYTE;

  if (index < size) {
    if (symbols_free < RMT_SYMBOLS_PER_BYTE) {
      return 0;
    }
    for (size_t i = 0; i < RMT_SYMBOLS_PER_BYTE; i++) {
      if (bytes[index] & (1 << (7 - i))) {
        symbols[i] = params->bit1;
      } else {
        symbols[i] = params->bit0;
      }
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
    if ((index + 1) >= size && params->reset.duration0 == 0 && params->reset.duration1 == 0) {
      *done = true;
    }
#endif
    return RMT_SYMBOLS_PER_BYTE;
  }

  if (symbols_free < 1) {
    return 0;
  }
  symbols[0] = params->reset;
  *done = true;
  return 1;
}
#endif

WS2805LightOutput::~WS2805LightOutput() {
  this->cleanup_();
}

void WS2805LightOutput::cleanup_() {
  delete[] this->buf_;
  this->buf_ = nullptr;
  delete[] this->effect_data_;
  this->effect_data_ = nullptr;
  delete[] this->rmt_buf_;
  this->rmt_buf_ = nullptr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (this->encoder_) {
    rmt_del_encoder(this->encoder_);
    this->encoder_ = nullptr;
  }
  if (this->channel_) {
    rmt_disable(this->channel_);
    rmt_del_channel(this->channel_);
    this->channel_ = nullptr;
  }
#else
  if (this->channel_ < RMT_CHANNEL_MAX) {
    rmt_driver_uninstall(this->channel_);
    this->channel_ = RMT_CHANNEL_MAX;
  }
#endif
}

void WS2805LightOutput::setup() {
  size_t buffer_size = this->num_leds_ * 5;

  this->buf_ = new uint8_t[buffer_size];
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    goto fail;
  }
  memset(this->buf_, 0, buffer_size);

  this->effect_data_ = new uint8_t[this->num_leds_];
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    goto fail;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  this->rmt_buf_ = new uint8_t[buffer_size];
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  this->rmt_buf_ = new rmt_symbol_word_t[buffer_size * 8 + 1];
#else
  this->rmt_buf_ = new rmt_item32_t[buffer_size * 8 + 1];
#endif
  if (this->rmt_buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate RMT buffer!");
    goto fail;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_tx_channel_config_t channel_cfg;
  memset(&channel_cfg, 0, sizeof(channel_cfg));
  channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  channel_cfg.resolution_hz = ws2805_rmt_resolution_hz();
  channel_cfg.gpio_num = gpio_num_t(this->pin_);
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
  channel_cfg.mem_block_symbols = 64; // Classic ESP32 and S2 require 64
#else
  channel_cfg.mem_block_symbols = 48; // WS2805 doesn't need huge blocks on modern chips
#endif
  channel_cfg.trans_queue_depth = 1;
  channel_cfg.flags.invert_out = 0;
  channel_cfg.flags.with_dma = 0;
  channel_cfg.intr_priority = 0;

  if (rmt_new_tx_channel(&channel_cfg, &this->channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Channel creation failed");
    goto fail;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  rmt_simple_encoder_config_t encoder_cfg;
  memset(&encoder_cfg, 0, sizeof(encoder_cfg));
  encoder_cfg.callback = ws2805_encoder_callback;
  encoder_cfg.arg = &this->params_;
  encoder_cfg.min_chunk_size = RMT_SYMBOLS_PER_BYTE;
  if (rmt_new_simple_encoder(&encoder_cfg, &this->encoder_) != ESP_OK) {
    ESP_LOGE(TAG, "Encoder creation failed");
    goto fail;
  }
#else
  rmt_copy_encoder_config_t encoder_cfg;
  memset(&encoder_cfg, 0, sizeof(encoder_cfg));
  if (rmt_new_copy_encoder(&encoder_cfg, &this->encoder_) != ESP_OK) {
    ESP_LOGE(TAG, "Encoder creation failed");
    goto fail;
  }
#endif

  if (rmt_enable(this->channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Enabling channel failed");
    goto fail;
  }

#else
  // For ESP-IDF 4.x RMT driver
  // Use ESPHome's built in real_channel loop behavior
  rmt_config_t rmt_cfg;
  rmt_cfg.rmt_mode = RMT_MODE_TX;
  rmt_cfg.gpio_num = gpio_num_t(this->pin_);
  rmt_cfg.mem_block_num = 1; // Explicitly 1 block as requested
  rmt_cfg.tx_config.loop_en = false;
  rmt_cfg.tx_config.idle_output_en = true;
  rmt_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_cfg.tx_config.carrier_en = false;
  rmt_cfg.clk_div = 8; // 80MHz / 8 = 10MHz = 100ns ticks

  this->channel_ = RMT_CHANNEL_MAX;
  for (int ch = 0; ch < RMT_CHANNEL_MAX; ch++) {
    rmt_cfg.channel = (rmt_channel_t)ch;
    esp_err_t err = rmt_config(&rmt_cfg);
    if (err == ESP_OK) {
      err = rmt_driver_install(rmt_cfg.channel, 0, ESP_INTR_FLAG_LOWMED);
      if (err == ESP_OK) {
        this->channel_ = (rmt_channel_t)ch;
        break;
      }
    }
  }

  if (this->channel_ == RMT_CHANNEL_MAX) {
    ESP_LOGE(TAG, "Failed to find free RMT channel");
    goto fail;
  }
#endif

  float ratio;
  ratio = (float) ws2805_rmt_resolution_hz() / 1e09f;

  // 0-bit: 300ns high, 900ns low
  this->params_.bit0.duration0 = (uint32_t) (ratio * 300);
  this->params_.bit0.level0 = 1;
  this->params_.bit0.duration1 = (uint32_t) (ratio * 900);
  this->params_.bit0.level1 = 0;
  // 1-bit: 900ns high, 300ns low
  this->params_.bit1.duration0 = (uint32_t) (ratio * 900);
  this->params_.bit1.level0 = 1;
  this->params_.bit1.duration1 = (uint32_t) (ratio * 300);
  this->params_.bit1.level1 = 0;
  // reset: 0ns high, 300us (300000ns) low
  this->params_.reset.duration0 = (uint32_t) (ratio * 0);
  this->params_.reset.level0 = 1;
  this->params_.reset.duration1 = (uint32_t) (ratio * 300000);
  this->params_.reset.level1 = 0;

  return;

fail:
  this->mark_failed();
  this->cleanup_();
}

void WS2805LightOutput::write_state(light::LightState *state) {
  if (this->buf_ == nullptr) return;

  float red, green, blue, cwhite, wwhite;
  state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite, true);

  uint8_t cw = cwhite * 255;
  uint8_t ww = wwhite * 255;

  for (int i = 0; i < this->size(); i++) {
    // GRBWW byte order: Offset 3 is W1 (Warm White typically), Offset 4 is W2 (Cold White typically).
    this->buf_[i * 5 + 3] = ww; // W1
    this->buf_[i * 5 + 4] = cw; // W2
  }

  // Workaround to ensure mark_shown_() doesn't unrequest the power supply if only CCT channels are active
  // Since ESPColorView in get_view_internal() has `white` mapped to nullptr, mark_shown_() will only see RGB values.
  uint8_t temp_g = this->buf_[0];
  if (cw > 0 || ww > 0) {
    this->buf_[0] = 1; // Temporarily trick mark_shown_() into seeing a non-zero pixel
  }

  this->mark_shown_();

  // Restore original pixel color
  this->buf_[0] = temp_g;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_err_t error = rmt_tx_wait_all_done(this->channel_, 1000);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->status_set_warning();
    return;
  }
#else
  rmt_wait_tx_done(this->channel_, 1000);
#endif
  delayMicroseconds(50);

  size_t buffer_size = this->num_leds_ * 5;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  memcpy(this->rmt_buf_, this->buf_, buffer_size);
#else
  size_t size = 0;
  size_t len = 0;
  uint8_t *psrc = this->buf_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_symbol_word_t *pdest = this->rmt_buf_;
#else
  rmt_item32_t *pdest = this->rmt_buf_;
#endif
  while (size < buffer_size) {
    uint8_t b = *psrc;
    for (int i = 0; i < 8; i++) {
      *pdest = b & (1 << (7 - i)) ? this->params_.bit1 : this->params_.bit0;
      pdest++;
      len++;
    }
    size++;
    psrc++;
  }

  // add reset symbol
  *pdest = this->params_.reset;
  pdest++;
  len++;
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  error = rmt_transmit(this->channel_, this->encoder_, this->rmt_buf_, buffer_size, &config);
#else
  error = rmt_transmit(this->channel_, this->encoder_, this->rmt_buf_, len * sizeof(rmt_symbol_word_t), &config);
#endif

  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX error");
    this->status_set_warning();
    return;
  }
#else
  // For ESP-IDF 4.x
  rmt_write_items(this->channel_, (rmt_item32_t *)this->rmt_buf_, len, true);
#endif
  this->status_clear_warning();
}

}  // namespace ws2805
}  // namespace esphome
