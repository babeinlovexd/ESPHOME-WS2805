#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/light/light_state.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_idf_version.h>
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

struct LedParams {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_symbol_word_t bit0;
  rmt_symbol_word_t bit1;
  rmt_symbol_word_t reset;
#else
  rmt_item32_t bit0;
  rmt_item32_t bit1;
  rmt_item32_t reset;
#endif
};

class WS2805LightOutput : public light::AddressableLight {
 public:
  WS2805LightOutput(uint16_t num_leds, uint8_t pin)
      : num_leds_(num_leds), pin_(pin) {}

  void setup() override;
  void write_state(light::LightState *state) override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    // Expose as an RGBWW light (RGB + Cold White + Warm White)
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
      for (int i = 0; i < this->size(); i++) {
        this->effect_data_[i] = 0;
      }
    }
  }

  void set_cold_white_temperature(float cold_white_temperature) {
    this->cold_white_temperature_ = cold_white_temperature;
  }
  void set_warm_white_temperature(float warm_white_temperature) {
    this->warm_white_temperature_ = warm_white_temperature;
  }
  void set_color_interlock(bool color_interlock) { this->color_interlock_ = color_interlock; }

 protected:
  static uint32_t ws2805_rmt_resolution_hz();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  static size_t ws2805_encoder_callback(const void *data, size_t size, size_t symbols_written, size_t symbols_free,
                                             rmt_symbol_word_t *symbols, bool *done, void *arg);
#endif

  light::ESPColorView get_view_internal(int32_t index) const override {
    if (this->buf_ == nullptr) {
      static uint8_t dummy[5] = {0};
      return light::ESPColorView(&dummy[0], &dummy[1], &dummy[2], nullptr, &dummy[4], &this->correction_);
    }

    // Hardware byte order is GRBWW (5 bytes per LED)
    uint8_t *base = this->buf_ + 5 * index;
    // Map R, G, B to ESPColorView. Leave White as nullptr.
    // ESPColorView wants R, G, B, W, cwhite
    // offset 0=G, 1=R, 2=B, 3=WW, 4=CW
    return light::ESPColorView(base + 1, base + 0, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
  }

  uint16_t num_leds_;
  uint8_t pin_;
  uint8_t *effect_data_{nullptr};
  float cold_white_temperature_{153};
  float warm_white_temperature_{500};
  bool color_interlock_{false};

  uint8_t *buf_{nullptr};
  LedParams params_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_channel_handle_t channel_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
#else
  rmt_channel_t channel_{RMT_CHANNEL_MAX}; // Need dynamic allocation in old ESP-IDF
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  uint8_t *rmt_buf_{nullptr};
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_symbol_word_t *rmt_buf_{nullptr};
#else
  rmt_item32_t *rmt_buf_{nullptr};
#endif
};

}  // namespace ws2805
}  // namespace esphome
