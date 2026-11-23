#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace esphome {
namespace ws2805 {

constexpr uint8_t SPARSE_MAX_SPACING = 8;
constexpr float SPARSE_PWM_REFERENCE = 1.0f;
constexpr float SPARSE_MAX_PWM = 255.0f;

struct SparseDimmingPlan {
  uint8_t step;
  float output_brightness;

  float apply_unscaled(float channel) const {
    return std::min(1.0f, std::max(0.0f, channel)) * this->output_brightness;
  }
};

inline SparseDimmingPlan make_sparse_dimming_plan(float brightness) {
  if (!(brightness > 0.0f))
    return {SPARSE_MAX_SPACING, 0.0f};
  if (brightness >= 1.0f)
    return {1, 1.0f};

  const float level = brightness * SPARSE_MAX_PWM;
  if (level <= SPARSE_MAX_SPACING) {
    // Keep the reference level fixed and reduce the spacing as brightness rises.
    const long step = std::lround(SPARSE_MAX_SPACING + 1.0f - level);
    const long spacing = std::min(static_cast<long>(SPARSE_MAX_SPACING), std::max(1L, step));
    return {static_cast<uint8_t>(spacing), SPARSE_PWM_REFERENCE / SPARSE_MAX_PWM};
  }

  // All pixels are lit at the join. Start the dense range at the same PWM
  // reference so the output never goes dark when crossing the threshold.
  const float dense_fraction = (level - SPARSE_MAX_SPACING) / (SPARSE_MAX_PWM - SPARSE_MAX_SPACING);
  const float pwm = SPARSE_PWM_REFERENCE + dense_fraction * (SPARSE_MAX_PWM - SPARSE_PWM_REFERENCE);
  return {1, std::min(pwm, SPARSE_MAX_PWM) / SPARSE_MAX_PWM};
}

// Share the leftover unlit pixels between the two ends of the strip.
inline int32_t sparse_first_pixel(int32_t pixel_count, uint8_t step) {
  return pixel_count > 0 ? ((pixel_count - 1) % step) / 2 : 0;
}

}  // namespace ws2805
}  // namespace esphome
