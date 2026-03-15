#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/light/light_state.h"
#include <NeoPixelBus.h>

namespace esphome {
namespace ws2805 {

class WS2805LightOutput : public light::LightOutput, public Component {
 public:
  WS2805LightOutput(uint16_t num_leds, uint8_t pin)
      : num_leds_(num_leds), pin_(pin) {}

  void setup() override {
    // We use NeoPixelBus dynamically allocated to pass constructor arguments
    this->controller_ = new NeoPixelBus<NeoGrbwwFeature, NeoWs2805Method>(this->num_leds_, this->pin_);
    this->controller_->Begin();
    this->controller_->Show();
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    // Expose as an RGBWW light (RGB + Cold White + Warm White)
    traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    return traits;
  }

  void write_state(light::LightState *state) override {
    float red, green, blue, cwhite, wwhite;
    // Get RGBWW values factoring in brightness
    // true = constant brightness logic used
    state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite, true);

    // Convert float (0.0 - 1.0) to uint8_t (0 - 255)
    uint8_t r = red * 255;
    uint8_t g = green * 255;
    uint8_t b = blue * 255;
    // For WS2805, W1 is typically WW and W2 is CW, depending on the manufacturer.
    // NeoGrbwwFeature expects (R, G, B, W1, W2) in RgbwwColor
    uint8_t cw = cwhite * 255;
    uint8_t ww = wwhite * 255;

    // We pass ww as W1 and cw as W2
    RgbwwColor color(r, g, b, ww, cw);

    for (uint16_t i = 0; i < this->num_leds_; i++) {
      this->controller_->SetPixelColor(i, color);
    }

    this->controller_->Show();
  }

 protected:
  uint16_t num_leds_;
  uint8_t pin_;
  NeoPixelBus<NeoGrbwwFeature, NeoWs2805Method> *controller_{nullptr};
};

}  // namespace ws2805
}  // namespace esphome
