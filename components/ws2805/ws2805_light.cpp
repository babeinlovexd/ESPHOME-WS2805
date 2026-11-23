#include "ws2805_light.h"
#include "ws2805_sparse_dimming.h"
#include "esphome/components/light/transformers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <esp_heap_caps.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "soc/soc_caps.h"  // SOC_RMT_SUPPORT_DMA, SOC_RMT_MEM_WORDS_PER_CHANNEL
#endif

namespace esphome {
namespace ws2805 {

static const char *const TAG = "ws2805";
static const size_t RMT_SYMBOLS_PER_BYTE = 8;

// ─────────────────────────────────────────────────────────────────────────────
//  setup() — orchestrates buffer allocation → RMT init → timing → boot clear
// ─────────────────────────────────────────────────────────────────────────────
void WS2805LightOutput::setup() {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
  if (this->rmt_no_dma_blocks_ == 4)
    ESP_LOGW(TAG, "rmt_no_dma_blocks: 4 requires ESP-IDF 5 or newer");
#elif SOC_RMT_SUPPORT_DMA
  if (this->rmt_no_dma_blocks_ == 4 && this->use_dma_ && this->fdin_pin_ < 0)
    ESP_LOGW(TAG, "rmt_no_dma_blocks: 4 applies only to RMT transmission without DMA");
#endif
  if (!this->allocate_buffers_()) {
    this->mark_failed();
    this->cleanup_();
    return;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (!this->init_rmt_channel_(this->din_pin_, "DIN", this->use_dma_, this->din_channel_, this->din_encoder_)) {
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // Reserve the DMA-capable TX channel for DIN. FDIN has its own RMT memory.
  if (this->fdin_pin_ >= 0 &&
      !this->init_rmt_channel_(this->fdin_pin_, "FDIN", false, this->fdin_channel_, this->fdin_encoder_)) {
    this->teardown_rmt_channel_(this->fdin_channel_, this->fdin_encoder_);
    ESP_LOGW(TAG, "FDIN backup unavailable on GPIO%d. Using DIN only.", this->fdin_pin_);
  }

  if (this->fdin_channel_ != nullptr) {
    // Where available, synchronize both channels carrying the same frame.
    // Otherwise the transmissions start back-to-back.
    rmt_channel_handle_t sync_channels[] = {this->din_channel_, this->fdin_channel_};
    rmt_sync_manager_config_t sync_cfg{};
    sync_cfg.tx_channel_array = sync_channels;
    sync_cfg.array_size = 2;
    esp_err_t sync_err = rmt_new_sync_manager(&sync_cfg, &this->sync_manager_);
    if (sync_err == ESP_OK) {
      ESP_LOGCONFIG(TAG, "  FDIN (DIN2) backup on GPIO%d — hardware-synchronized with DIN", this->fdin_pin_);
    } else {
      this->sync_manager_ = nullptr;
      ESP_LOGCONFIG(TAG, "  FDIN (DIN2) backup on GPIO%d — unsynchronized (%s)", this->fdin_pin_,
                    esp_err_to_name(sync_err));
    }
  }
#else
  if (!this->init_rmt_channel_(this->din_pin_, "DIN", this->din_channel_)) {
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // FDIN (DIN2) backup channel. The legacy driver has no sync manager, so the
  // two channels are launched back-to-back in encode_and_transmit_ (best effort).
  if (this->fdin_pin_ >= 0) {
    if (this->init_rmt_channel_(this->fdin_pin_, "FDIN", this->fdin_channel_)) {
      ESP_LOGCONFIG(TAG, "  FDIN (DIN2) backup on GPIO%d", this->fdin_pin_);
    } else {
      ESP_LOGW(TAG, "FDIN backup unavailable on GPIO%d. Using DIN only.", this->fdin_pin_);
    }
  }
#endif

  this->calc_timing_();
  this->setup_complete_ = true;

  // Send a full zero frame on boot to clear any chips left in an unknown state
  // by a previous firmware (incomplete frames, stale data, etc.).
  this->transmit_clear_frame_();
}

bool WS2805LightOutput::allocate_buffers_() {
  size_t buffer_size = this->frame_bytes_();

  this->buf_ = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    return false;
  }
  memset(this->buf_, 0, buffer_size);

  this->effect_data_ = (uint8_t *)heap_caps_malloc(this->num_leds_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    return false;
  }
  memset(this->effect_data_, 0, this->num_leds_);

  size_t rmt_buffer_size = (buffer_size * RMT_SYMBOLS_PER_BYTE + 1) * sizeof(RmtSymbol);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  if (!this->rmt_preencode_)
    rmt_buffer_size = buffer_size;
#endif
  this->rmt_buf_ = static_cast<decltype(this->rmt_buf_)>(
      heap_caps_malloc(rmt_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (this->rmt_buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate %zu bytes of internal RAM for the RMT transmit buffer", rmt_buffer_size);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    if (this->rmt_preencode_)
      ESP_LOGE(TAG, "Try rmt_preencode: false or reduce the number of pixels");
#endif
    return false;
  }

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  this->color_accessor_.set_buffers(this->buf_, this->effect_data_, this->num_leds_);
  this->accessor_ = &this->color_accessor_;
#endif
  return true;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
bool WS2805LightOutput::init_rmt_channel_(int pin, const char *name, bool use_dma, rmt_channel_handle_t &channel,
                                          rmt_encoder_handle_t &encoder) {
  rmt_tx_channel_config_t channel_cfg{};
  channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  channel_cfg.resolution_hz = rmt_resolution_hz_();
  channel_cfg.gpio_num = gpio_num_t(pin);
  channel_cfg.trans_queue_depth = 1;
#if !SOC_RMT_SUPPORT_DMA
  use_dma = false;
#endif
  // With DMA this sizes its buffer; without DMA it reserves hardware memory.
  channel_cfg.flags.with_dma = use_dma;
  channel_cfg.mem_block_symbols = (use_dma ? 4 : this->rmt_no_dma_blocks_) * SOC_RMT_MEM_WORDS_PER_CHANNEL;
  // Use the configured interrupt priority for timely refills.
  channel_cfg.intr_priority = this->isr_priority_;

  esp_err_t err = rmt_new_tx_channel(&channel_cfg, &channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s RMT channel creation failed (%s, %zu symbols): %s", name,
             use_dma ? "DMA" : "without DMA", channel_cfg.mem_block_symbols, esp_err_to_name(err));
    if (!use_dma && err == ESP_ERR_NOT_FOUND && this->rmt_no_dma_blocks_ == 4)
      ESP_LOGE(TAG, "Try rmt_no_dma_blocks: 2 or reduce other RMT usage");
    return false;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  if (!this->rmt_preencode_) {
    rmt_simple_encoder_config_t encoder_cfg{};
    encoder_cfg.callback = encoder_callback_;
    encoder_cfg.arg = &this->params_;
    encoder_cfg.min_chunk_size = RMT_SYMBOLS_PER_BYTE;
    err = rmt_new_simple_encoder(&encoder_cfg, &encoder);
  } else
#endif
  {
    rmt_copy_encoder_config_t encoder_cfg{};
    err = rmt_new_copy_encoder(&encoder_cfg, &encoder);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s encoder creation failed: %s", name, esp_err_to_name(err));
    return false;
  }

  if (rmt_enable(channel) != ESP_OK) {
    ESP_LOGE(TAG, "Enabling %s channel failed", name);
    return false;
  }

  ESP_LOGCONFIG(TAG, "  %s RMT on GPIO%d: %s, %zu symbols", name, pin,
                use_dma ? "DMA" : "without DMA", channel_cfg.mem_block_symbols);
  return true;
}
#else
bool WS2805LightOutput::init_rmt_channel_(int pin, const char *name, rmt_channel_t &channel) {
  rmt_channel_status_result_t channel_status{};
  esp_err_t err = rmt_get_channel_status(&channel_status);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot check RMT channels for %s: %s", name, esp_err_to_name(err));
    return false;
  }

  rmt_config_t rmt_cfg{};
  rmt_cfg.rmt_mode = RMT_MODE_TX;
  rmt_cfg.gpio_num = gpio_num_t(pin);
  rmt_cfg.mem_block_num = 1;
  rmt_cfg.tx_config.loop_en = false;
  rmt_cfg.tx_config.idle_output_en = true;
  rmt_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_cfg.tx_config.carrier_en = false;
  // 80MHz APB / 8 = 10MHz resolution, matching rmt_resolution_hz_() for IDF <5.0.
  rmt_cfg.clk_div = 8;

  for (int ch = 0; ch < RMT_CHANNEL_MAX; ch++) {
    if (channel_status.status[ch] != RMT_CHANNEL_UNINIT)
      continue;
    rmt_cfg.channel = (rmt_channel_t)ch;
    if (rmt_config(&rmt_cfg) == ESP_OK && rmt_driver_install(rmt_cfg.channel, 0, ESP_INTR_FLAG_LOWMED) == ESP_OK) {
      channel = rmt_cfg.channel;
      return true;
    }
  }

  ESP_LOGE(TAG, "Failed to find free RMT channel for %s", name);
  return false;
}
#endif

uint32_t WS2805LightOutput::rmt_resolution_hz_() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  uint32_t freq = 0;
  if (esp_clk_tree_src_get_freq_hz((soc_module_clk_t) RMT_CLK_SRC_DEFAULT,
                                   ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq) != ESP_OK ||
      freq == 0) {
    // Query failed — fall back to the 80MHz APB clock the RMT uses by default.
    freq = 80000000;
  }
  return freq;
#else
  return 10000000;
#endif
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
size_t IRAM_ATTR HOT WS2805LightOutput::encoder_callback_(const void *data, size_t size, size_t symbols_written, size_t symbols_free,
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

void WS2805LightOutput::calc_timing_() {
  float ratio = (float) rmt_resolution_hz_() / 1e09f;

  ESP_LOGCONFIG(TAG, "  RMT Resolution: %" PRIu32 " Hz (Ratio: %f)", rmt_resolution_hz_(), ratio);

  // Round to nearest tick — bounds error to ±0.5 ticks regardless of RMT clock freq
  this->params_.bit0.duration0 = (uint32_t) (ratio * this->bit0_high_ns_ + 0.5f);
  this->params_.bit0.level0 = 1;
  this->params_.bit0.duration1 = (uint32_t) (ratio * this->bit0_low_ns_ + 0.5f);
  this->params_.bit0.level1 = 0;
  this->params_.bit1.duration0 = (uint32_t) (ratio * this->bit1_high_ns_ + 0.5f);
  this->params_.bit1.level0 = 1;
  this->params_.bit1.duration1 = (uint32_t) (ratio * this->bit1_low_ns_ + 0.5f);
  this->params_.bit1.level1 = 0;
  // RMT symbol duration fields are 15-bit (max 32767 ticks). A long reset can
  // exceed that at high RMT resolutions (e.g. >409µs @ 80MHz), so split the low
  // period across both halves of the symbol — both held low — to double the
  // range, then clamp. Short resets keep duration1 = 0 (unchanged behavior).
  uint32_t reset_ticks = (uint32_t) (ratio * this->reset_pulse_us_ * 1000.0f + 0.5f);
  uint32_t reset_d0 = reset_ticks > 32767u ? 32767u : reset_ticks;
  uint32_t reset_d1 = reset_ticks - reset_d0;
  if (reset_d1 > 32767u) reset_d1 = 32767u;
  this->params_.reset.duration0 = reset_d0;
  this->params_.reset.level0 = 0;
  this->params_.reset.duration1 = reset_d1;
  this->params_.reset.level1 = 0;
}

void WS2805LightOutput::transmit_clear_frame_() {
  memset(this->buf_, 0, this->frame_bytes_());
  this->encode_and_transmit_();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Frame rendering
// ─────────────────────────────────────────────────────────────────────────────
namespace {
class WS2805Transition final : public light::LightTransitionTransformer {
 public:
  optional<light::LightColorValues> apply() override {
    // A mode change passes through black, including at the exact midpoint.
    if (this->changing_color_mode_ && this->get_progress_() == 0.5f)
      return this->intermediate_values_;
    return light::LightTransitionTransformer::apply();
  }
};
}  // namespace

std::unique_ptr<light::LightTransformer> WS2805LightOutput::create_default_transition() {
  // Keep current_values in step with the rendered frame for all five channels.
  return make_unique<WS2805Transition>();
}

void WS2805LightOutput::update_state(light::LightState *state) {
  if (!this->setup_complete_)
    return;

  const auto &values = state->current_values;
  const float brightness = values.get_state() * values.get_brightness();
  this->correction_.set_local_brightness(light::to_uint8_scale(brightness));

  if (this->sparse_dimming_ && !this->is_effect_active() && state->get_current_effect_index() == 0) {
    this->update_sparse_state_(values, brightness);
  } else {
    this->sparse_step_ = 1;
    state->current_values_as_cwww(&this->cold_white_, &this->warm_white_, this->constant_brightness_);
    if (!this->is_effect_active()) {
      const bool rgb_mode = values.get_color_mode() & light::ColorCapability::RGB;
      this->all() = rgb_mode ? light::color_from_light_color_values(values) : Color{};
    }
  }

  this->schedule_show();
}

void WS2805LightOutput::update_sparse_state_(light::LightColorValues values, float brightness) {
  const auto plan = make_sparse_dimming_plan(brightness);
  this->sparse_step_ = plan.step;
  values.set_state(true);
  values.set_brightness(1.0f);

  float red, green, blue, cold_white, warm_white;
  values.as_rgbww(&red, &green, &blue, &cold_white, &warm_white, this->constant_brightness_);
  this->cold_white_ = plan.apply_unscaled(cold_white);
  this->warm_white_ = plan.apply_unscaled(warm_white);
  const auto r = light::to_uint8_scale(plan.apply_unscaled(red));
  const auto g = light::to_uint8_scale(plan.apply_unscaled(green));
  const auto b = light::to_uint8_scale(plan.apply_unscaled(blue));
  int32_t next_lit = sparse_first_pixel(this->size(), plan.step);
  for (int32_t i = 0; i < this->size(); ++i) {
    uint8_t *pixel = this->buf_ + BYTES_PER_LED * i;
    const bool lit = i == next_lit;
    pixel[this->offset_r_] = lit ? r : 0;
    pixel[this->offset_g_] = lit ? g : 0;
    pixel[this->offset_b_] = lit ? b : 0;
    if (lit)
      next_lit += plan.step;
  }
}

void WS2805LightOutput::write_state(light::LightState *state) {
  if (!this->setup_complete_)
    return;

  uint8_t cw, ww;
  if (this->dithering_ && state->is_transformer_active()) {
    cw = this->calculate_dither_(this->cold_white_ * 255.0f, this->error_cw_);
    ww = this->calculate_dither_(this->warm_white_ * 255.0f, this->error_ww_);
  } else {
    cw = light::to_uint8_scale(this->cold_white_);
    ww = light::to_uint8_scale(this->warm_white_);
    this->error_cw_ = 0.0f;
    this->error_ww_ = 0.0f;
  }

  // Keep RGB written by effects, partitions and addressable_set intact.
  if (this->color_interlock_ && (cw != 0 || ww != 0) && this->has_rgb_pixels_())
    cw = ww = 0;
  this->write_whites_(cw, ww);

#ifdef USE_POWER_SUPPLY
  // The inherited RGB accessor cannot report either WS2805 white channel.
  if (cw != 0 || ww != 0)
    this->power_.request();
  else
    this->mark_shown_();
#endif

  if (this->encode_and_transmit_())
    this->status_clear_warning();
}

bool WS2805LightOutput::has_rgb_pixels_() const {
  for (int32_t i = 0; i < this->size(); ++i) {
    const uint8_t *pixel = this->buf_ + BYTES_PER_LED * i;
    if (pixel[this->offset_r_] || pixel[this->offset_g_] || pixel[this->offset_b_])
      return true;
  }
  return false;
}

void WS2805LightOutput::write_whites_(uint8_t cw, uint8_t ww) {
  int32_t next_lit = sparse_first_pixel(this->size(), this->sparse_step_);
  for (int32_t i = 0; i < this->size(); ++i) {
    uint8_t *pixel = this->buf_ + BYTES_PER_LED * i;
    const bool lit = i == next_lit;
    pixel[this->offset_w1_] = lit ? ww : 0;
    pixel[this->offset_w2_] = lit ? cw : 0;
    if (lit)
      next_lit += this->sparse_step_;
  }
}

uint8_t WS2805LightOutput::calculate_dither_(float base, float &error) {
  const float desired = base + error;
  const auto value = static_cast<uint8_t>(std::round(std::clamp(desired, 0.0f, 255.0f)));
  error = base <= 0.0f || base >= 255.0f ? 0.0f : desired - value;
  return value;
}

// Encode buf_ into rmt_buf_. Returns the length to hand to the transmit call:
//   simple encoder (IDF 5.3+) → raw byte count
//   copy encoder (IDF 5+)     → encoded symbol byte count
//   v4.x     → item count        (rmt_write_items)
size_t WS2805LightOutput::encode_buffer_() {
  const size_t buffer_size = this->frame_bytes_();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  if (!this->rmt_preencode_) {
    memcpy(this->rmt_buf_, this->buf_, buffer_size);
    return buffer_size;
  }
#endif
  RmtSymbol *symbols = reinterpret_cast<RmtSymbol *>(this->rmt_buf_);
  RmtSymbol *next = symbols;
  for (size_t i = 0; i < buffer_size; i++) {
    const uint8_t byte = this->buf_[i];
    for (uint8_t bit = 0x80; bit != 0; bit >>= 1)
      *next++ = (byte & bit) ? this->params_.bit1 : this->params_.bit0;
  }
  *next++ = this->params_.reset;
  const size_t symbol_count = next - symbols;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  return symbol_count * sizeof(RmtSymbol);
#else
  return symbol_count;
#endif
}

// Drain any in-flight TX, encode the current buffer, and launch it on DIN and
// (when configured) the FDIN backup line.
//
// FDIN carries the SAME frame as DIN, feeding the WS2805 backup (BI) input for
// breakpoint-resume redundancy: if one IC's data-out fails, the next IC falls
// back to BI and the rest of the strand keeps working. When the SoC supports it
// the RMT sync manager bit-aligns the two lines; otherwise they launch back-to-
// back (harmless skew, since a chip uses DI when healthy).
bool WS2805LightOutput::encode_and_transmit_() {
  int timeout_ms = this->tx_timeout_ms_();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  // Phase 1: drain previous TX on both lines before touching rmt_buf_.
  if (rmt_tx_wait_all_done(this->din_channel_, timeout_ms) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->tx_error_count_++;
    this->status_set_warning();
    return false;
  }
  if (this->fdin_channel_ != nullptr && rmt_tx_wait_all_done(this->fdin_channel_, timeout_ms) != ESP_OK) {
    ESP_LOGE(TAG, "FDIN TX timeout");
    this->tx_error_count_++;
    this->status_set_warning();
    return false;
  }

  // Frame render cost = encode + transmit + wait. The Phase 1 drain is excluded
  // because it waits on the *previous* frame, not this one. Feeds the diagnostics.
  int64_t frame_start = esp_timer_get_time();

  // Phase 2: encode buf_ → rmt_buf_.
  size_t tx_size = this->encode_buffer_();

  // Phase 3: launch DIN (+ synchronized FDIN backup) and wait for completion.
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));

  // Re-arm the sync manager so the two queued transmissions start together and
  // stay bit-aligned; the channels don't fire until both are queued.
  if (this->sync_manager_ != nullptr)
    rmt_sync_reset(this->sync_manager_);

  if (rmt_transmit(this->din_channel_, this->din_encoder_, this->rmt_buf_, tx_size, &config) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX error");
    this->tx_error_count_++;
    this->status_set_warning();
    return false;
  }
  if (this->fdin_channel_ != nullptr &&
      rmt_transmit(this->fdin_channel_, this->fdin_encoder_, this->rmt_buf_, tx_size, &config) != ESP_OK) {
    ESP_LOGE(TAG, "FDIN TX error");
    this->tx_error_count_++;
    this->status_set_warning();
    return false;
  }

  if (rmt_tx_wait_all_done(this->din_channel_, timeout_ms) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->tx_error_count_++;
    this->status_set_warning();
    return false;
  }
  if (this->fdin_channel_ != nullptr && rmt_tx_wait_all_done(this->fdin_channel_, timeout_ms) != ESP_OK) {
    ESP_LOGE(TAG, "FDIN TX timeout");
    this->tx_error_count_++;
    this->status_set_warning();
    return false;
  }
  this->last_frame_us_ = esp_timer_get_time() - frame_start;
#else  // ESP-IDF < 5.0 — legacy driver, no sync manager
  int64_t frame_start = esp_timer_get_time();
  size_t len = this->encode_buffer_();
  // Launch both non-blocking so they overlap as closely as the driver allows,
  // then wait for both to finish.
  rmt_write_items(this->din_channel_, this->rmt_buf_, len, false);
  if (this->fdin_channel_ < RMT_CHANNEL_MAX)
    rmt_write_items(this->fdin_channel_, this->rmt_buf_, len, false);
  rmt_wait_tx_done(this->din_channel_, pdMS_TO_TICKS(timeout_ms));
  if (this->fdin_channel_ < RMT_CHANNEL_MAX)
    rmt_wait_tx_done(this->fdin_channel_, pdMS_TO_TICKS(timeout_ms));
  this->last_frame_us_ = esp_timer_get_time() - frame_start;
#endif

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Teardown
// ─────────────────────────────────────────────────────────────────────────────
WS2805LightOutput::~WS2805LightOutput() {
  this->cleanup_();
}

void WS2805LightOutput::cleanup_() {
  this->setup_complete_ = false;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  this->accessor_ = &light::NullColorAccessor::instance;
  this->color_accessor_.set_buffers(nullptr, nullptr, 0);
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  // The sync manager must be deleted before the channels it manages.
  if (this->sync_manager_ != nullptr) {
    rmt_del_sync_manager(this->sync_manager_);
    this->sync_manager_ = nullptr;
  }
  this->teardown_rmt_channel_(this->din_channel_, this->din_encoder_);
  this->teardown_rmt_channel_(this->fdin_channel_, this->fdin_encoder_);
#else
  this->teardown_rmt_channel_(this->din_channel_);
  this->teardown_rmt_channel_(this->fdin_channel_);
#endif
  // Both channels can reference the shared transmit buffer.
  if (this->rmt_buf_) heap_caps_free(this->rmt_buf_);
  this->rmt_buf_ = nullptr;
  if (this->effect_data_) heap_caps_free(this->effect_data_);
  this->effect_data_ = nullptr;
  if (this->buf_) heap_caps_free(this->buf_);
  this->buf_ = nullptr;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void WS2805LightOutput::teardown_rmt_channel_(rmt_channel_handle_t &channel, rmt_encoder_handle_t &encoder) {
  if (channel) {
    rmt_disable(channel);
    rmt_del_channel(channel);
    channel = nullptr;
  }
  if (encoder) {
    rmt_del_encoder(encoder);
    encoder = nullptr;
  }
}
#else
void WS2805LightOutput::teardown_rmt_channel_(rmt_channel_t &channel) {
  if (channel < RMT_CHANNEL_MAX) {
    rmt_driver_uninstall(channel);
    channel = RMT_CHANNEL_MAX;
  }
}
#endif

}
}
