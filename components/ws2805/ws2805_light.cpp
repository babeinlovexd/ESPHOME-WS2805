#include "ws2805_light.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <esp_heap_caps.h>
#include <cmath>

namespace esphome {
namespace ws2805 {

static const char *const TAG = "ws2805";
static const size_t RMT_SYMBOLS_PER_BYTE = 8;

uint8_t WS2805LightOutput::calculate_dither_(float base, float &error) {
  float desired = base + error;
  uint8_t val;
  if (desired >= 255.0f) val = 255;
  else if (desired <= 0.0f) val = 0;
  else val = static_cast<uint8_t>(std::round(desired));

  if (base <= 0.0f || base >= 255.0f) error = 0.0f;
  else error = desired - static_cast<float>(val);

  return val;
}

uint32_t WS2805LightOutput::ws2805_rmt_resolution_hz() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  uint32_t freq;
  esp_clk_tree_src_get_freq_hz((soc_module_clk_t) RMT_CLK_SRC_DEFAULT, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq);
  return freq;
#else
  return 10000000;
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
  if (this->buf_) heap_caps_free(this->buf_);
  this->buf_ = nullptr;
  if (this->effect_data_) heap_caps_free(this->effect_data_);
  this->effect_data_ = nullptr;
  if (this->rmt_buf_) heap_caps_free(this->rmt_buf_);
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

  this->buf_ = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    goto fail;
  }
  memset(this->buf_, 0, buffer_size);

  this->effect_data_ = (uint8_t *)heap_caps_malloc(this->num_leds_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    goto fail;
  }
  memset(this->effect_data_, 0, this->num_leds_);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  this->rmt_buf_ = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  this->rmt_buf_ = (rmt_symbol_word_t *)heap_caps_malloc((buffer_size * 8 + 1) * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  this->rmt_buf_ = (rmt_item32_t *)heap_caps_malloc((buffer_size * 8 + 1) * sizeof(rmt_item32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
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
  channel_cfg.mem_block_symbols = 64;
#else
  channel_cfg.mem_block_symbols = 48;
#endif
  channel_cfg.trans_queue_depth = 1;
  channel_cfg.flags.invert_out = 0;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  channel_cfg.flags.with_dma = 1;
#else
  channel_cfg.flags.with_dma = 0;
#endif
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
  rmt_config_t rmt_cfg;
  rmt_cfg.rmt_mode = RMT_MODE_TX;
  rmt_cfg.gpio_num = gpio_num_t(this->pin_);
  rmt_cfg.mem_block_num = 1;
  rmt_cfg.tx_config.loop_en = false;
  rmt_cfg.tx_config.idle_output_en = true;
  rmt_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_cfg.tx_config.carrier_en = false;
  rmt_cfg.clk_div = this->rmt_clk_div_;

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

  ESP_LOGCONFIG(TAG, "  RMT Resolution: %" PRIu32 " Hz (Ratio: %f)", ws2805_rmt_resolution_hz(), ratio);

  // Round to nearest tick — bounds error to ±0.5 ticks regardless of RMT clock freq
  this->params_.bit0.duration0 = (uint32_t) (ratio * this->bit0_high_ns_ + 0.5f);
  this->params_.bit0.level0 = 1;
  this->params_.bit0.duration1 = (uint32_t) (ratio * this->bit0_low_ns_ + 0.5f);
  this->params_.bit0.level1 = 0;
  this->params_.bit1.duration0 = (uint32_t) (ratio * this->bit1_high_ns_ + 0.5f);
  this->params_.bit1.level0 = 1;
  this->params_.bit1.duration1 = (uint32_t) (ratio * this->bit1_low_ns_ + 0.5f);
  this->params_.bit1.level1 = 0;
  this->params_.reset.duration0 = 0;  // always zero — hi-duration of reset symbol is unused
  this->params_.reset.level0 = 1;
  this->params_.reset.duration1 = (uint32_t) (ratio * this->reset_pulse_us_ * 1000 + 0.5f);
  this->params_.reset.level1 = 0;

  return;

fail:
  this->mark_failed();
  this->cleanup_();
}

void WS2805LightOutput::write_state(light::LightState *state) {
  if (this->buf_ == nullptr) return;

  uint32_t now = micros();
  if (this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < this->max_refresh_rate_) {
    this->schedule_show();
    return;
  }

  float dt = (now - this->last_refresh_) / 1000000.0f;
  this->last_refresh_ = now;
  if (dt > 0.5f) dt = 0.0f;


  float target_r, target_g, target_b, target_cw, target_ww;
  state->remote_values.as_rgbww(&target_r, &target_g, &target_b, &target_cw, &target_ww, this->constant_brightness_);

  ESP_LOGV("ws2805", "mode=%d r=%.2f g=%.2f b=%.2f cw=%.2f ww=%.2f", (int)state->current_values.get_color_mode(), target_r, target_g, target_b, target_cw, target_ww);

  bool clear_rgb = false;

  if (this->color_interlock_) {
    auto color_mode = state->current_values.get_color_mode();
    if (color_mode == light::ColorMode::COLD_WARM_WHITE) {
      clear_rgb = true;
    } else if (color_mode == light::ColorMode::RGB) {
      target_cw = 0.0f;
      target_ww = 0.0f;
      this->current_cw_ = 0.0f;  // void: snap whites to zero — prevents lingering CCT values
      this->current_ww_ = 0.0f;  //       from previous white mode bleeding into RGB effects
      this->step_cw_ = 0.0f;
      this->step_ww_ = 0.0f;
    }
  }



  if (this->target_cw_internal_ != target_cw || this->target_ww_internal_ != target_ww) {
    this->target_cw_internal_ = target_cw;
    this->target_ww_internal_ = target_ww;


    ESP_LOGD(TAG, "White Channel Hardware Targets -> CW: %.1f%%, WW: %.1f%%", target_cw * 100.0f, target_ww * 100.0f);

    if (this->transition_speed_ <= 0.0f) {
      this->current_cw_ = target_cw;
      this->current_ww_ = target_ww;
      this->step_cw_ = 0.0f;
      this->step_ww_ = 0.0f;
    } else {

      this->step_cw_ = (target_cw - this->current_cw_) / this->transition_speed_;
      this->step_ww_ = (target_ww - this->current_ww_) / this->transition_speed_;
    }
  }


  if (this->current_cw_ != this->target_cw_internal_) {
    float move_cw = this->step_cw_ * dt;

    if ((this->step_cw_ > 0 && this->current_cw_ + move_cw >= this->target_cw_internal_) ||
        (this->step_cw_ < 0 && this->current_cw_ + move_cw <= this->target_cw_internal_)) {
      this->current_cw_ = this->target_cw_internal_;
    } else {
      this->current_cw_ += move_cw;
    }
  }


  if (this->current_ww_ != this->target_ww_internal_) {
    float move_ww = this->step_ww_ * dt;
    if ((this->step_ww_ > 0 && this->current_ww_ + move_ww >= this->target_ww_internal_) ||
        (this->step_ww_ < 0 && this->current_ww_ + move_ww <= this->target_ww_internal_)) {
      this->current_ww_ = this->target_ww_internal_;
    } else {
      this->current_ww_ += move_ww;
    }
  }


  bool is_fading = (std::abs(this->current_cw_ - target_cw) > 0.001f || std::abs(this->current_ww_ - target_ww) > 0.001f);


  float base_cw = this->current_cw_ * 255.0f;
  float base_ww = this->current_ww_ * 255.0f;
  uint8_t cw, ww;

  if (this->dithering_ && is_fading) {
    cw = this->calculate_dither_(base_cw, this->error_cw_);
    ww = this->calculate_dither_(base_ww, this->error_ww_);
  } else {

    cw = static_cast<uint8_t>(std::round(base_cw));
    ww = static_cast<uint8_t>(std::round(base_ww));
    this->error_cw_ = 0.0f;
    this->error_ww_ = 0.0f;
  }

  int n = this->size();
  if (clear_rgb) {
    for (int i = 0; i < n; i++) {
      this->buf_[i * 5 + this->offset_r_] = 0;
      this->buf_[i * 5 + this->offset_g_] = 0;
      this->buf_[i * 5 + this->offset_b_] = 0;
      this->buf_[i * 5 + this->offset_w1_] = ww;
      this->buf_[i * 5 + this->offset_w2_] = cw;
    }
  } else {
    for (int i = 0; i < n; i++) {
      this->buf_[i * 5 + this->offset_w1_] = ww;
      this->buf_[i * 5 + this->offset_w2_] = cw;
    }
  }

  uint8_t temp_g = this->buf_[0];
  if (cw > 0 || ww > 0) {
    this->buf_[0] = 1;
  }

  this->mark_shown_();

  this->buf_[0] = temp_g;

  int timeout_ms = (static_cast<uint32_t>(this->num_leds_) * 50) / 1000 + 50;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_err_t error = rmt_tx_wait_all_done(this->channel_, timeout_ms);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->status_set_warning();
    return;
  }
#else
  rmt_wait_tx_done(this->channel_, pdMS_TO_TICKS(timeout_ms));
#endif
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
  rmt_write_items(this->channel_, (rmt_item32_t *)this->rmt_buf_, len, true);
#endif
  this->status_clear_warning();


  if (is_fading) {
    this->schedule_show();
  }
}

}
}
