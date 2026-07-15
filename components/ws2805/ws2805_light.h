#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/light/light_state.h"

#include <cstring>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/rmt_tx.h>
#else
#include <driver/rmt.h>
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <esp_clk_tree.h>
#endif

namespace esphome {
namespace ws2805 {

// Layout-compatible RMT symbol type across IDF versions — one alias avoids
// repeating the #if guard for every symbol declaration below.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
using RmtSymbol = rmt_symbol_word_t;
#else
using RmtSymbol = rmt_item32_t;
#endif

struct LedParams {
  RmtSymbol bit0;
  RmtSymbol bit1;
  RmtSymbol reset;
};

class WS2805LightOutput : public light::AddressableLight {
 public:
  WS2805LightOutput(uint16_t num_leds, uint8_t din_pin)
      : num_leds_(num_leds), din_pin_(din_pin), fdin_pin_(0), buf_(nullptr), effect_data_(nullptr), rmt_buf_(nullptr) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    this->din_channel_ = nullptr;
    this->din_encoder_ = nullptr;
    this->fdin_channel_ = nullptr;
    this->fdin_encoder_ = nullptr;
    this->sync_manager_ = nullptr;
#else
    this->din_channel_ = RMT_CHANNEL_MAX;
    this->fdin_channel_ = RMT_CHANNEL_MAX;
#endif
  }
  ~WS2805LightOutput();

  void setup() override;
  void write_state(light::LightState *state) override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->color_interlock_) {
      traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLD_WARM_WHITE});
    } else {
      traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    }
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);

    return traits;
  }

  int32_t size() const override { return this->num_leds_; }

  void clear_effect_data() override {
    if (this->effect_data_) {
      memset(this->effect_data_, 0, this->size());
    }
    // A buffer-modifying effect is (re)starting and now owns the RGB buffer.
    // Effects never touch the white (WW/CW) channels — the color view exposes
    // RGB only. Suppress any residual white and reset the CCT fade state, but
    // allow the user to mix CW/WW back in while the effect is running.
    this->effect_whites_suppressed_ = true;
    this->effect_whites_captured_ = false;
    this->clear_white_state_();
  }

  void set_cold_white_temperature(float cold_white_temperature) {
    this->cold_white_temperature_ = cold_white_temperature;
  }
  void set_warm_white_temperature(float warm_white_temperature) {
    this->warm_white_temperature_ = warm_white_temperature;
  }
  void set_color_interlock(bool color_interlock) { this->color_interlock_ = color_interlock; }
  void set_transition_speed(uint32_t speed_ms) { this->transition_speed_ = speed_ms / 1000.0f; }
  void set_dithering(bool dithering) { this->dithering_ = dithering; }
  void set_bit0_high_ns(uint32_t ns) { this->bit0_high_ns_ = ns; }
  void set_bit0_low_ns(uint32_t ns) { this->bit0_low_ns_ = ns; }
  void set_bit1_high_ns(uint32_t ns) { this->bit1_high_ns_ = ns; }
  void set_bit1_low_ns(uint32_t ns) { this->bit1_low_ns_ = ns; }
  void set_reset_pulse_us(uint32_t us) { this->reset_pulse_us_ = us; }
  void set_isr_priority(uint8_t priority) { this->isr_priority_ = priority; }
  void set_channel_order(uint8_t r, uint8_t g, uint8_t b, uint8_t w1, uint8_t w2) {
    this->offset_r_ = r;
    this->offset_g_ = g;
    this->offset_b_ = b;
    this->offset_w1_ = w1;
    this->offset_w2_ = w2;
  }
  void set_constant_brightness(bool constant_brightness) { this->constant_brightness_ = constant_brightness; }
  void set_fdin_pin(uint8_t pin) { this->fdin_pin_ = pin; }

  // ── Diagnostics (read from YAML template sensors via output_id) ──
  // Number of WS2805 pixels driven on this strand.
  uint16_t get_num_leds() const { return this->num_leds_; }
  // Bytes clocked out per frame (num_leds × 5 channels).
  uint32_t get_frame_bytes() const { return this->frame_bytes_(); }
  // Detected RMT tick clock (Hz) — the resolution all bit timing is derived from.
  uint32_t get_rmt_resolution_hz() const { return rmt_resolution_hz_(); }
  // Blocking cost of the last frame (encode + RMT transmit + wait), in ms.
  float get_last_frame_ms() const { return this->last_frame_us_ / 1000.0f; }
  // Theoretical ceiling: how many of those frames fit in one second.
  float get_max_refresh_hz() const {
    return this->last_frame_us_ > 0 ? 1e6f / (float) this->last_frame_us_ : 0.0f;
  }
  // Cumulative RMT TX timeouts/errors since boot — should stay 0 in healthy operation.
  uint32_t get_tx_error_count() const { return this->tx_error_count_; }

 private:
  uint8_t calculate_dither_(float base, float &error);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  static size_t encoder_callback_(const void *data, size_t size, size_t symbols_written, size_t symbols_free,
                                  rmt_symbol_word_t *symbols, bool *done, void *arg);
#endif

 protected:
  void cleanup_();
  static uint32_t rmt_resolution_hz_();

  size_t frame_bytes_() const { return static_cast<size_t>(this->num_leds_) * BYTES_PER_LED; }
  // Worst-case time to clock out one frame: ~50µs/LED plus a fixed 50ms margin.
  int tx_timeout_ms_() const { return (static_cast<uint32_t>(this->num_leds_) * 50) / 1000 + 50; }

  // ── setup() helpers (callstack order) ──
  bool allocate_buffers_();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  bool init_rmt_channel_(uint8_t pin, const char *name, rmt_channel_handle_t &channel,
                         rmt_encoder_handle_t &encoder);
  void teardown_rmt_channel_(rmt_channel_handle_t &channel, rmt_encoder_handle_t &encoder);
#else
  bool init_rmt_channel_(uint8_t pin, const char *name, rmt_channel_t &channel, rmt_channel_t skip);
  void teardown_rmt_channel_(rmt_channel_t &channel);
#endif
  void calc_timing_();
  void transmit_clear_frame_();

  // ── write_state() helpers (callstack order) ──
  void read_target_values_(light::LightState *state, float &r, float &g, float &b, float &cw, float &ww);
  void update_white_transition_(float target_cw, float target_ww, float dt);
  static void step_channel_(float &current, float target, float step, float dt);
  void compute_white_bytes_(bool is_fading, uint8_t &cw, uint8_t &ww);
  void clear_white_state_();
  void fill_led_buffer_(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw);
  size_t encode_buffer_();
  bool encode_and_transmit_();

  light::ESPColorView get_view_internal(int32_t index) const override {
    if (this->buf_ == nullptr) {
      return light::ESPColorView(&this->dummy_view_[0], &this->dummy_view_[1], &this->dummy_view_[2], nullptr,
                                 &this->dummy_view_[4], &this->correction_);
    }

    uint8_t *base = this->buf_ + BYTES_PER_LED * index;
    return light::ESPColorView(
        base + this->offset_r_,
        base + this->offset_g_,
        base + this->offset_b_,
        nullptr,
        this->effect_data_ + index,
        &this->correction_
    );
  }

  static constexpr size_t BYTES_PER_LED = 5;  // WS2805 = R, G, B, W1, W2

  uint16_t num_leds_;
  mutable uint8_t dummy_view_[BYTES_PER_LED]{};  // returned by get_view_internal() before buf_ is allocated
  bool setup_complete_{false};  // true after setup() succeeds — guards write_state() against pre-setup calls
  uint8_t offset_r_{1};
  uint8_t offset_g_{0};
  uint8_t offset_b_{2};
  uint8_t offset_w1_{3};
  uint8_t offset_w2_{4};
  uint8_t din_pin_;
  uint8_t fdin_pin_{0};  // 0 = disabled; else the WS2805 backup (BI) line, driven in sync with DIN
  uint8_t *effect_data_{nullptr};
  float cold_white_temperature_{153};
  float warm_white_temperature_{500};
  bool color_interlock_{false};
  float current_cw_{0.0f};
  float current_ww_{0.0f};
  float transition_speed_{3.0f};
  float target_cw_internal_{-1.0f};
  float target_ww_internal_{-1.0f};
  float step_cw_{0.0f};
  float step_ww_{0.0f};
  int64_t last_show_us_{0};  // esp_timer_get_time() at previous frame; drives per-frame fade dt
  int64_t last_frame_us_{0};  // duration of the last encode+transmit — diagnostic
  uint32_t tx_error_count_{0};  // cumulative RMT TX timeouts/errors — diagnostic
  bool dithering_{false};
  bool constant_brightness_{false};
  float error_cw_{0.0f};
  float error_ww_{0.0f};
  bool effect_whites_suppressed_{false};
  bool effect_whites_captured_{false};
  float suppressed_cw_{0.0f};
  float suppressed_ww_{0.0f};

  // Configurable RMT bit timing (defaults: WS2805 datasheet center values).
  // Tick counts are resolution-dependent; the "@80MHz" notes below are just
  // illustrative for the classic ESP32 (resolution is auto-detected per target).
  uint32_t bit0_high_ns_{300};   // spec: 220-380ns → center at 300ns (24 ticks @ 80MHz)
  uint32_t bit0_low_ns_{800};    // spec: 580ns-1µs → 800ns (64 ticks @ 80MHz)
  uint32_t bit1_high_ns_{800};   // spec: 580ns-1µs → 800ns (64 ticks @ 80MHz)
  uint32_t bit1_low_ns_{800};    // spec: 580ns-1µs → 800ns (64 ticks @ 80MHz)
  uint32_t reset_pulse_us_{300}; // spec: ≥280µs (24000 ticks @ 80MHz)
  uint8_t isr_priority_{3};      // RMT TX interrupt priority (1-3)

  uint8_t *buf_{nullptr};
  LedParams params_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_channel_handle_t din_channel_{nullptr};
  rmt_encoder_handle_t din_encoder_{nullptr};
  rmt_channel_handle_t fdin_channel_{nullptr};
  rmt_encoder_handle_t fdin_encoder_{nullptr};
  rmt_sync_manager_handle_t sync_manager_{nullptr};
#else
  rmt_channel_t din_channel_{RMT_CHANNEL_MAX};
  rmt_channel_t fdin_channel_{RMT_CHANNEL_MAX};
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  uint8_t *rmt_buf_{nullptr};
#else
  RmtSymbol *rmt_buf_{nullptr};
#endif
};

}
}
