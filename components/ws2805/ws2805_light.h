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
    this->controller_ = new NeoPixelBus<NeoGrbwwFeature, NeoWs2805Method>(this->num_leds_, this->pin_);
    this->controller_->Begin();
    this->effect_data_ = new uint8_t[this->num_leds_](); // initialize to 0
    this->controller_->Show();
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    // Expose as an RGBWW light (RGB + Cold White + Warm White)
    traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    return traits;
  }

  int32_t size() const override { return this->num_leds_; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++) {
      this->effect_data_[i] = 0;
    }
  }

  void write_state(light::LightState *state) override {
    float red, green, blue, cwhite, wwhite;
    // Get RGBWW values factoring in brightness. We use the global state for the CCT channels.
    state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite, true);

    uint8_t cw = cwhite * 255;
    uint8_t ww = wwhite * 255;

    // The AddressableLight wrapper has already updated the R, G, B bytes in the NeoPixelBus buffer
    // via our ESPColorView mappings!
    // However, since AddressableLight does not understand the 5th channel, we manually inject
    // the global Warm White and Cold White values into every pixel before showing!

    uint8_t *pixels = this->controller_->Pixels();
    for (int i = 0; i < this->size(); i++) {
      // NeoGrbwwFeature byte order is: G, R, B, W1, W2
      // We set W1 to Warm White and W2 to Cold White.
      pixels[i * 5 + 3] = ww; // W1
      pixels[i * 5 + 4] = cw; // W2
    }

    this->mark_shown_();
    this->controller_->Dirty();
    this->controller_->Show();
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    uint8_t *base = this->controller_->Pixels() + 5ULL * index;
    // Map G, R, B to ESPColorView. Leave White as nullptr.
    // NeoGrbwwFeature byte order is: G, R, B, W1, W2 -> offset 0=G, 1=R, 2=B
    return light::ESPColorView(base + 1, base + 0, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
  }

  uint16_t num_leds_;
  uint8_t pin_;
  uint8_t *effect_data_{nullptr};
  NeoPixelBus<NeoGrbwwFeature, NeoWs2805Method> *controller_{nullptr};
};

}  // namespace ws2805
}  // namespace esphome
