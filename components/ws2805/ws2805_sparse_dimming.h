#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace esphome {
namespace ws2805 {

struct SparseDimmingPlan {
  float brightness;
  float pwm_level;
  uint8_t step;

  float apply(float channel) const {
    if (brightness <= 0.0f || channel <= 0.0f)
      return 0.0f;
    if (brightness >= 1.0f)
      return std::min(channel, 1.0f);
    // Divide first, rather than multiplying by 1/brightness: even very small
    // positive inputs must not overflow the scale or turn zero channels into NaN.
    return std::min(channel / brightness, 1.0f) * (pwm_level / 255.0f);
  }
};

inline SparseDimmingPlan make_sparse_dimming_plan(float brightness, uint8_t max_step, uint8_t floor_pwm) {
  brightness = std::max(0.0f, std::min(brightness, 1.0f));
  max_step = std::max(uint8_t(2), std::min(max_step, uint8_t(32)));
  floor_pwm = std::max(uint8_t(1), std::min(floor_pwm, uint8_t(255)));
  if (brightness <= 0.0f)
    return {0.0f, 0.0f, max_step};
  if (brightness >= 1.0f)
    return {1.0f, 255.0f, 1};

  const float level = brightness * 255.0f;
  if (level <= max_step) {
    const auto step = static_cast<uint8_t>(std::max(1.0f, std::min(float(max_step),
                                                               std::round(max_step - level + 1.0f))));
    return {brightness, static_cast<float>(floor_pwm), step};
  }
  // Join at the floor PWM (all pixels active), then increase to PWM 255.
  // Starting the dense interval at zero would create a blackout when brightness
  // increases.
  const float pwm_level = static_cast<float>(floor_pwm) +
                          (level - max_step) * ((255.0f - static_cast<float>(floor_pwm)) /
                                                (255.0f - max_step));
  return {brightness, pwm_level, 1};
}

}  // namespace ws2805
}  // namespace esphome
