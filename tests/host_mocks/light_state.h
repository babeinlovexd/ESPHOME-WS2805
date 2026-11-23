#pragma once

#include "esphome/components/light/light_color_values.h"
#include "esphome_runtime.h"

namespace esphome::light {

class LightOutput;

// Drives the actual output and transformer at explicit times without the
// ESPHome scheduler. Color math and pixel operations come from ESPHome.
class LightState {
 public:
  explicit LightState(LightOutput *output = nullptr) : output_(output) {
    for (size_t i = 0; i < this->gamma_.size(); ++i) {
      const float corrected = this->gamma_correct_lut(static_cast<float>(i) / 255.0f);
      this->gamma_[i] = static_cast<uint16_t>(std::round(corrected * 65535.0f));
    }
  }

  LightOutput *get_output() { return this->output_; }
  bool is_transformer_active() const { return this->transformer_active; }
  uint16_t get_current_effect_index() const { return this->effect_index; }
  const uint16_t *get_gamma_table() const { return this->gamma_.data(); }
  float gamma_correct_lut(float value) const { return std::pow(value, 2.8f); }
  void current_values_as_cwww(float *cold, float *warm, bool constant_brightness);
  void schedule_write_() { this->pending_write = true; }
  uint32_t get_flash_transition_length() const { return 0; }
  void publish_state() { ++this->publications; }

  LightColorValues current_values, remote_values;
  bool transformer_active{false};
  bool pending_write{false};
  uint16_t effect_index{0};
  unsigned publications{0};

 private:
  LightOutput *output_;
  std::array<uint16_t, 256> gamma_{};
};

}  // namespace esphome::light
