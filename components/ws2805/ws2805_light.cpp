#include "ws2805_light.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <esp_heap_caps.h>
#include <cmath>
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
  if (!this->allocate_buffers_()) {
    this->mark_failed();
    this->cleanup_();
    return;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (!this->init_rmt_channel_(this->din_pin_, "DIN", this->din_channel_, this->din_encoder_)) {
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // FDIN (DIN2) backup channel — driven with the SAME frame as DIN, bit-aligned
  // via the RMT sync manager, so it feeds the WS2805 breakpoint-resume backup
  // (BI) input in lock-step with the primary (DI) input.
  if (this->fdin_pin_ != 0) {
    if (!this->init_rmt_channel_(this->fdin_pin_, "FDIN", this->fdin_channel_, this->fdin_encoder_)) {
      this->mark_failed();
      this->cleanup_();
      return;
    }

    // Bit-align DIN and FDIN via the RMT sync manager where the SoC supports it
    // (e.g. ESP32-S3). The classic ESP32 has no TX-synchro hardware, so this
    // returns ESP_ERR_NOT_SUPPORTED — that's fine: we fall back to launching
    // both channels back-to-back (a few µs of skew, harmless since a WS2805 uses
    // DI when healthy and only reads BI on DI failure). A backup-line limitation
    // must NEVER take down the primary DIN output.
    rmt_channel_handle_t sync_channels[] = {this->din_channel_, this->fdin_channel_};
    rmt_sync_manager_config_t sync_cfg;
    memset(&sync_cfg, 0, sizeof(sync_cfg));
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
  if (!this->init_rmt_channel_(this->din_pin_, "DIN", this->din_channel_, RMT_CHANNEL_MAX)) {
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // FDIN (DIN2) backup channel. The legacy driver has no sync manager, so the
  // two channels are launched back-to-back in encode_and_transmit_ (best effort).
  if (this->fdin_pin_ != 0) {
    if (!this->init_rmt_channel_(this->fdin_pin_, "FDIN", this->fdin_channel_, this->din_channel_)) {
      this->mark_failed();
      this->cleanup_();
      return;
    }
    ESP_LOGCONFIG(TAG, "  FDIN (DIN2) backup on GPIO%d", this->fdin_pin_);
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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  this->rmt_buf_ = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  this->rmt_buf_ = (rmt_symbol_word_t *)heap_caps_malloc((buffer_size * 8 + 1) * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  this->rmt_buf_ = (rmt_item32_t *)heap_caps_malloc((buffer_size * 8 + 1) * sizeof(rmt_item32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
  if (this->rmt_buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate RMT buffer!");
    return false;
  }

  return true;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
bool WS2805LightOutput::init_rmt_channel_(uint8_t pin, const char *name, rmt_channel_handle_t &channel,
                                          rmt_encoder_handle_t &encoder) {
  rmt_tx_channel_config_t channel_cfg;
  memset(&channel_cfg, 0, sizeof(channel_cfg));
  channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  channel_cfg.resolution_hz = rmt_resolution_hz_();
  channel_cfg.gpio_num = gpio_num_t(pin);
  channel_cfg.trans_queue_depth = 1;
  channel_cfg.flags.invert_out = 0;
  // Capability-detect DMA and memory sizing from the SoC caps so every build
  // target is handled correctly — not just the ones we enumerate by name.
#if SOC_RMT_SUPPORT_DMA
  // DMA refills the transmitter without CPU involvement, so an ISR-starved
  // refill can't underrun the line; a modest buffer is plenty. Keep it a
  // multiple of the per-channel word count so it's valid on every DMA target.
  channel_cfg.flags.with_dma = 1;
  channel_cfg.mem_block_symbols = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;
#else
  // No DMA: the frame streams through on-chip RMT memory blocks, refilled by the
  // ISR mid-frame. Two blocks give headroom so a late refill (WiFi/BLE/web_server
  // ISR contention) can't underrun the data line — an underrun stalls it high and
  // downstream WS2805 chips latch 0xFF on every channel = full white.
  channel_cfg.flags.with_dma = 0;
  channel_cfg.mem_block_symbols = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;
#endif
  // Give the RMT refill ISR high priority so it preempts the WiFi/BLE stack.
  // At priority 0 (low) a busy radio starves the refill and the strip glitches
  // to white mid-frame. 3 is the highest priority available to a C-level ISR.
  channel_cfg.intr_priority = 3;

  if (rmt_new_tx_channel(&channel_cfg, &channel) != ESP_OK) {
    ESP_LOGE(TAG, "%s channel creation failed", name);
    return false;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  rmt_simple_encoder_config_t encoder_cfg;
  memset(&encoder_cfg, 0, sizeof(encoder_cfg));
  encoder_cfg.callback = encoder_callback_;
  encoder_cfg.arg = &this->params_;
  encoder_cfg.min_chunk_size = RMT_SYMBOLS_PER_BYTE;
  if (rmt_new_simple_encoder(&encoder_cfg, &encoder) != ESP_OK) {
    ESP_LOGE(TAG, "%s encoder creation failed", name);
    return false;
  }
#else
  rmt_copy_encoder_config_t encoder_cfg;
  memset(&encoder_cfg, 0, sizeof(encoder_cfg));
  if (rmt_new_copy_encoder(&encoder_cfg, &encoder) != ESP_OK) {
    ESP_LOGE(TAG, "%s encoder creation failed", name);
    return false;
  }
#endif

  if (rmt_enable(channel) != ESP_OK) {
    ESP_LOGE(TAG, "Enabling %s channel failed", name);
    return false;
  }

  return true;
}
#else
bool WS2805LightOutput::init_rmt_channel_(uint8_t pin, const char *name, rmt_channel_t &channel, rmt_channel_t skip) {
  rmt_config_t rmt_cfg;
  rmt_cfg.rmt_mode = RMT_MODE_TX;
  rmt_cfg.gpio_num = gpio_num_t(pin);
  rmt_cfg.mem_block_num = 1;
  rmt_cfg.tx_config.loop_en = false;
  rmt_cfg.tx_config.idle_output_en = true;
  rmt_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_cfg.tx_config.carrier_en = false;
  // 80MHz APB / 8 = 10MHz resolution, matching rmt_resolution_hz_() for IDF <5.0.
  rmt_cfg.clk_div = 8;

  channel = RMT_CHANNEL_MAX;
  for (int ch = 0; ch < RMT_CHANNEL_MAX; ch++) {
    if (skip != RMT_CHANNEL_MAX && (rmt_channel_t)ch == skip) continue;  // skip already-used channel
    rmt_cfg.channel = (rmt_channel_t)ch;
    esp_err_t err = rmt_config(&rmt_cfg);
    if (err == ESP_OK) {
      err = rmt_driver_install(rmt_cfg.channel, 0, ESP_INTR_FLAG_LOWMED);
      if (err == ESP_OK) {
        channel = (rmt_channel_t)ch;
        break;
      }
    }
  }

  if (channel == RMT_CHANNEL_MAX) {
    ESP_LOGE(TAG, "Failed to find free RMT channel for %s", name);
    return false;
  }

  return true;
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
//  write_state() — read → transition → dither → fill buffer → transmit
// ─────────────────────────────────────────────────────────────────────────────
void WS2805LightOutput::write_state(light::LightState *state) {
  if (this->buf_ == nullptr || !this->setup_complete_) return;

  // A buffer-modifying effect (rainbow, color wipe, twinkle, ...) owns buf_: it
  // writes per-LED RGB through the color views each frame. Flattening the strip
  // to a single color here would erase the effect and, depending on the frame
  // timing relative to the incoming command, leak a stale solid color (e.g.
  // warm white rendering as green). Transmit exactly what the effect drew. The
  // white bytes stay zero: cleared in clear_effect_data() when the effect
  // started, and effects never touch them (the color view exposes RGB only).
  if (this->is_effect_active()) {
    this->mark_shown_();
    if (!this->encode_and_transmit_()) return;
    this->status_clear_warning();
    return;
  }

  float target_r, target_g, target_b, target_cw, target_ww;
  this->read_target_values_(state, target_r, target_g, target_b, target_cw, target_ww);

  // Elapsed time since the previous frame drives the hardware-side white fade.
  // Clamp to 0.1s so a long idle gap can't produce a single huge step (which
  // would snap instead of fade) the next time the light changes.
  int64_t now_us = esp_timer_get_time();
  float dt = (this->last_show_us_ == 0) ? 0.0f : (now_us - this->last_show_us_) / 1e6f;
  if (dt > 0.1f) dt = 0.1f;
  this->last_show_us_ = now_us;
  this->update_white_transition_(target_cw, target_ww, dt);

  bool is_fading = (std::abs(this->current_cw_ - target_cw) > 0.001f ||
                    std::abs(this->current_ww_ - target_ww) > 0.001f);

  uint8_t cw, ww;
  this->compute_white_bytes_(is_fading, cw, ww);

  uint8_t r8 = static_cast<uint8_t>(std::round(target_r * 255.0f));
  uint8_t g8 = static_cast<uint8_t>(std::round(target_g * 255.0f));
  uint8_t b8 = static_cast<uint8_t>(std::round(target_b * 255.0f));

  // No effect active (handled by the early return above): write ALL 5 channels
  // from remote_values. buf_ is the single source of truth for the solid-color
  // path — overriding whatever update_state filled — so the true target state
  // is always shown.
  this->fill_led_buffer_(r8, g8, b8, ww, cw);

  this->mark_shown_();

  if (!this->encode_and_transmit_()) return;

  this->status_clear_warning();

  if (is_fading) {
    this->schedule_show();
  }
}

void WS2805LightOutput::read_target_values_(light::LightState *state, float &r, float &g, float &b, float &cw, float &ww) {
  state->remote_values.as_rgbww(&r, &g, &b, &cw, &ww, this->constant_brightness_);

  if (this->color_interlock_) {
    auto color_mode = state->remote_values.get_color_mode();
    if (color_mode == light::ColorMode::COLD_WARM_WHITE) {
      r = g = b = 0.0f;
    } else {
      cw = 0.0f;
      ww = 0.0f;
      this->current_cw_ = 0.0f;
      this->current_ww_ = 0.0f;
      this->step_cw_ = 0.0f;
      this->step_ww_ = 0.0f;
    }
  }
}

void WS2805LightOutput::update_white_transition_(float target_cw, float target_ww, float dt) {
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

  this->step_channel_(this->current_cw_, this->target_cw_internal_, this->step_cw_, dt);
  this->step_channel_(this->current_ww_, this->target_ww_internal_, this->step_ww_, dt);
}

void WS2805LightOutput::step_channel_(float &current, float target, float step, float dt) {
  if (current == target) return;

  float move = step * dt;
  if ((step > 0 && current + move >= target) ||
      (step < 0 && current + move <= target)) {
    current = target;
  } else {
    current += move;
  }
}

void WS2805LightOutput::compute_white_bytes_(bool is_fading, uint8_t &cw, uint8_t &ww) {
  float base_cw = this->current_cw_ * 255.0f;
  float base_ww = this->current_ww_ * 255.0f;

  if (this->dithering_ && is_fading) {
    cw = this->calculate_dither_(base_cw, this->error_cw_);
    ww = this->calculate_dither_(base_ww, this->error_ww_);
  } else {
    cw = static_cast<uint8_t>(std::round(base_cw));
    ww = static_cast<uint8_t>(std::round(base_ww));
    this->error_cw_ = 0.0f;
    this->error_ww_ = 0.0f;
  }
}

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

// Zero the white (WW/CW) bytes for every LED and reset the CCT fade internals.
// Called when a buffer-modifying effect starts (via clear_effect_data), so no
// residual warm/cold white bleeds into the effect. The fade targets are reset
// to their sentinel (-1) so the next white command recomputes the transition
// from a known-zero baseline.
void WS2805LightOutput::clear_white_state_() {
  if (this->buf_ != nullptr) {
    int n = this->size();
    for (int i = 0; i < n; i++) {
      uint8_t *p = this->buf_ + BYTES_PER_LED * i;
      p[this->offset_w1_] = 0;
      p[this->offset_w2_] = 0;
    }
  }
  this->current_cw_ = 0.0f;
  this->current_ww_ = 0.0f;
  this->target_cw_internal_ = -1.0f;
  this->target_ww_internal_ = -1.0f;
  this->step_cw_ = 0.0f;
  this->step_ww_ = 0.0f;
  this->error_cw_ = 0.0f;
  this->error_ww_ = 0.0f;
}

void WS2805LightOutput::fill_led_buffer_(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw) {
  int n = this->size();
  for (int i = 0; i < n; i++) {
    uint8_t *p = this->buf_ + BYTES_PER_LED * i;
    p[this->offset_r_] = r;
    p[this->offset_g_] = g;
    p[this->offset_b_] = b;
    p[this->offset_w1_] = ww;
    p[this->offset_w2_] = cw;
  }
}

// Encode buf_ into rmt_buf_. Returns the length to hand to the transmit call:
//   v5.3+    → byte count        (simple encoder consumes raw bytes)
//   v5.0-5.2 → symbol byte count (copy encoder consumes pre-expanded symbols)
//   v4.x     → item count        (rmt_write_items)
size_t WS2805LightOutput::encode_buffer_() {
  size_t buffer_size = this->frame_bytes_();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  memcpy(this->rmt_buf_, this->buf_, buffer_size);
  return buffer_size;
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  size_t sym_count = 0;
  uint8_t *psrc = this->buf_;
  RmtSymbol *pdest = this->rmt_buf_;
  for (size_t i = 0; i < buffer_size; i++) {
    uint8_t b = *psrc++;
    for (int j = 0; j < 8; j++) {
      *pdest++ = (b & (1 << (7 - j))) ? this->params_.bit1 : this->params_.bit0;
      sym_count++;
    }
  }
  *pdest++ = this->params_.reset;
  sym_count++;
  return sym_count * sizeof(RmtSymbol);
#else
  size_t len = 0;
  uint8_t *psrc = this->buf_;
  RmtSymbol *pdest = this->rmt_buf_;
  for (size_t i = 0; i < buffer_size; i++) {
    uint8_t b = *psrc++;
    for (int j = 0; j < 8; j++) {
      *pdest++ = (b & (1 << (7 - j))) ? this->params_.bit1 : this->params_.bit0;
      len++;
    }
  }
  *pdest++ = this->params_.reset;
  len++;
  return len;
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
  if (this->buf_) heap_caps_free(this->buf_);
  this->buf_ = nullptr;
  if (this->effect_data_) heap_caps_free(this->effect_data_);
  this->effect_data_ = nullptr;
  if (this->rmt_buf_) heap_caps_free(this->rmt_buf_);
  this->rmt_buf_ = nullptr;
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
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void WS2805LightOutput::teardown_rmt_channel_(rmt_channel_handle_t &channel, rmt_encoder_handle_t &encoder) {
  if (encoder) {
    rmt_del_encoder(encoder);
    encoder = nullptr;
  }
  if (channel) {
    rmt_disable(channel);
    rmt_del_channel(channel);
    channel = nullptr;
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
