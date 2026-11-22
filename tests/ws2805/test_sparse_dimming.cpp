#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "ws2805_sparse_dimming.h"

using esphome::ws2805::make_sparse_dimming_plan;
static int quantize(float value) { return static_cast<int>(std::round(value * 255.0f)); }

int main() {
  const std::array<float, 7> ratios{{0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f}};
  // Every allowed floor and spacing, with fractional brightness and STRICT
  // monotonicity. A whole PWM code of jitter must never be silently accepted.
  for (int floor = 1; floor <= 255; floor++) for (uint8_t spacing = 2; spacing <= 32; spacing++) {
    std::array<int, 7> previous{};
    uint8_t previous_step = spacing;
    for (int level = 0; level <= 510; level++) {
      const auto plan = make_sparse_dimming_plan(level / 510.0f, spacing, uint8_t(floor));
      assert(plan.step >= 1 && plan.step <= previous_step);
      previous_step = plan.step;
      for (size_t channel = 0; channel < ratios.size(); channel++) {
        const float output = plan.apply_unscaled(ratios[channel]);
        assert(std::isfinite(output) && output >= 0.0f && output <= 1.0f);
        const int code = quantize(output);
        assert(code >= previous[channel]);
        previous[channel] = code;
      }
    }
    const auto before = make_sparse_dimming_plan(spacing / 255.0f, spacing, uint8_t(floor));
    const auto after = make_sparse_dimming_plan((spacing + 0.1f) / 255.0f, spacing, uint8_t(floor));
    assert(before.step == 1 && after.step == 1);
    assert(quantize(before.apply_unscaled(1)) == floor);
    assert(quantize(after.apply_unscaled(1)) >= floor);
    assert(after.pwm_level >= before.pwm_level);
  }
  // Dense sampling for both defaults, including mixed-color rounding ties.
  for (uint8_t floor : std::array<uint8_t,2>{{1,2}}) for (uint8_t spacing = 2; spacing <= 32; spacing++) {
    std::array<int, 7> previous{};
    for (int level = 0; level <= 25500; level++) {
      const auto plan = make_sparse_dimming_plan(level / 25500.0f, spacing, floor);
      const int lit = (42 - 1) / plan.step + 1;
      for (size_t channel = 0; channel < ratios.size(); channel++) {
        const int energy = quantize(plan.apply_unscaled(ratios[channel])) * lit;
        assert(energy >= previous[channel]);
        previous[channel] = energy;
      }
    }
  }
  for (uint8_t floor : std::array<uint8_t,5>{{1,2,32,128,255}}) {
    const auto off = make_sparse_dimming_plan(0, 8, floor);
    assert(off.apply_unscaled(0) == 0 && off.apply_unscaled(1) == 0);
    const auto tiny = make_sparse_dimming_plan(std::numeric_limits<float>::denorm_min(), 8, floor);
    assert(quantize(tiny.apply_unscaled(1)) == floor && tiny.apply_unscaled(0) == 0);
    const auto full = make_sparse_dimming_plan(1, 32, floor);
    assert(full.step == 1);
    for (int value = 0; value <= 255; value++) {
      const float channel = value / 255.0f;
      assert(full.apply_unscaled(channel) == channel);
    }
  }
  assert(make_sparse_dimming_plan(0.01f, 0, 0).step <= 2);
  assert(make_sparse_dimming_plan(0.01f, 255, 255).step <= 32);
  std::cout << "Sparse curve: all 255 floors, 31 spacings and strict mixed-color monotonicity passed\n";
}
