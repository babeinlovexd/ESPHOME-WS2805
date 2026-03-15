#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/light/light_state.h"
#include <NeoPixelBus.h>

namespace esphome {
namespace ws2805 {

class WS2805LightOutput : public light::AddressableLight {
 public:
  WS2805LightOutput(uint16_t num_leds, uint8_t pin)
      : num_leds_(num_leds), pin_(pin) {}

  void setup() override {
    // The datasheet implies R, G, B, W1, W2 byte order.
    // NeoRgbwwFeature ensures exactly this mapping!
    this->controller_ = new NeoPixelBus<NeoRgbwwFeature, NeoWs2805Method>(this->num_leds_, this->pin_);
    this->controller_->Begin();
    this->effect_data_ = new uint8_t[this->num_leds_](); // initialize to 0
    this->controller_->Show();
  }

  // CRITICAL: Ensure setup is called early so this->controller_ is allocated before state restore calls write_state!
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
    for (int i = 0; i < this->size(); i++) {
      this->effect_data_[i] = 0;
    }
  }

  void write_state(light::LightState *state) override {
    if (this->controller_ == nullptr) return; // Failsafe

    float red, green, blue, cwhite, wwhite;
    // Get RGBWW values factoring in brightness. We use the global state for the CCT channels.
    state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite, true);

    uint8_t cw = cwhite * 255;
    uint8_t ww = wwhite * 255;

    // VERY IMPORTANT: Call the base class write_state!
    // If the light is NOT running an effect, AddressableLight doesn't implement write_state itself,
    // but the underlying LightOutput interface expects us to push to hardware.
    // Wait, AddressableLight DOES NOT implement write_state()! It's pure virtual in LightOutput!
    // But we need to make sure the transformer has applied the color to our ESPColorView mappings!
    // The transformer applies to `(*this)[i]` before calling `write_state()` on the output.
    // So by the time `write_state` is called, R, G, B in the NeoPixelBus buffer are already updated!

    uint8_t *pixels = this->controller_->Pixels();
    if (pixels != nullptr) {
      for (int i = 0; i < this->size(); i++) {
        // NeoRgbwwFeature byte order is: R, G, B, W1, W2
        // Offset 3 is W1 (Warm White typically), Offset 4 is W2 (Cold White typically).
        pixels[i * 5 + 3] = ww; // W1
        pixels[i * 5 + 4] = cw; // W2
      }
    }

    this->mark_shown_();
    this->controller_->Dirty();
    this->controller_->Show();
  }

  void set_cold_white_temperature(float cold_white_temperature) {
    this->cold_white_temperature_ = cold_white_temperature;
  }
  void set_warm_white_temperature(float warm_white_temperature) {
    this->warm_white_temperature_ = warm_white_temperature;
  }
  void set_color_interlock(bool color_interlock) { this->color_interlock_ = color_interlock; }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    if (this->controller_ == nullptr) {
      // Failsafe if accessed before setup, though ESPHome shouldn't do this anymore thanks to get_setup_priority
      static uint8_t dummy[5] = {0};
      return light::ESPColorView(&dummy[0], &dummy[1], &dummy[2], nullptr, &dummy[4], &this->correction_);
    }

    uint8_t *base = this->controller_->Pixels() + 5ULL * index;
    // Map R, G, B to ESPColorView. Leave White as nullptr.
    // NeoRgbwwFeature byte order is: R, G, B, W1, W2 -> offset 0=R, 1=G, 2=B
    return light::ESPColorView(base + 0, base + 1, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
  }

  uint16_t num_leds_;
  uint8_t pin_;
  uint8_t *effect_data_{nullptr};
  float cold_white_temperature_{153};
  float warm_white_temperature_{500};
  bool color_interlock_{false};
  NeoPixelBus<NeoRgbwwFeature, NeoWs2805Method> *controller_{nullptr};
};

}  // namespace ws2805
}  // namespace esphome
