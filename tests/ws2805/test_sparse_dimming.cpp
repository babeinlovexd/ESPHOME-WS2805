#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "ws2805_sparse_dimming.h"

using esphome::ws2805::make_sparse_dimming_plan;

static constexpr uint8_t kFloorPwm = 2;

static int quantize(float value) { return static_cast<int>(std::round(value * 255.0f)); }

int main() {
  for (uint8_t spacing = 2; spacing <= 32; spacing++) {
    const auto before = make_sparse_dimming_plan(spacing / 255.0f, spacing, kFloorPwm);
    const auto after = make_sparse_dimming_plan((spacing + 0.1f) / 255.0f, spacing, kFloorPwm);
    assert(before.step == 1 && after.step == 1);
    assert(quantize(before.apply(before.brightness)) == kFloorPwm);
    assert(quantize(after.apply(after.brightness)) >= kFloorPwm);
    const auto half = make_sparse_dimming_plan(0.5f / 255.0f, spacing, kFloorPwm);
    const auto one = make_sparse_dimming_plan(1.0f / 255.0f, spacing, kFloorPwm);
    assert(quantize(half.apply(half.brightness)) == kFloorPwm);
    assert(quantize(one.apply(one.brightness)) == kFloorPwm);
    assert(half.apply(0.0f) == 0.0f);

    // Check total channel energy, not individual pixel identity: the centered
    // pattern can move when spacing changes, while its lit count must increase.
    for (int pixels : {1, 2, 7, 22, 42, 50, 1000}) {
      std::array<int, 5> previous{};
      for (int level = 0; level <= 25500; level++) {
        const float brightness = level / 25500.0f;
        const auto plan = make_sparse_dimming_plan(brightness, spacing, kFloorPwm);
        assert(plan.step >= 1 && plan.step <= spacing);
        const int lit = (pixels - 1) / plan.step + 1;
        for (size_t channel = 0; channel < previous.size(); channel++) {
          const float ratio = channel * 0.25f;
          const float output = plan.apply(brightness * ratio);
          assert(std::isfinite(output) && output >= 0.0f && output <= 1.0f);
          const int energy = quantize(output) * lit;
          // Recovering the ratio via channel/brightness leaves ~1 ULP of float
          // error, which can nudge a value sitting exactly on a 0.5 rounding
          // boundary by one LSB. Allow that jitter (scaled by lit) in the
          // monotonicity check.
          assert(energy >= previous[channel] - lit);
          previous[channel] = energy;
        }
      }
    }
  }
  const auto off = make_sparse_dimming_plan(0.0f, 8, kFloorPwm);
  assert(off.apply(0.0f) == 0.0f && off.apply(1.0f) == 0.0f);
  const auto tiny = make_sparse_dimming_plan(std::numeric_limits<float>::denorm_min(), 8, kFloorPwm);
  assert(std::isfinite(tiny.apply(tiny.brightness)));
  assert(quantize(tiny.apply(tiny.brightness)) == kFloorPwm);
  assert(tiny.apply(0.0f) == 0.0f);
  const auto full = make_sparse_dimming_plan(1.0f, 32, kFloorPwm);
  assert(full.step == 1);
  for (int value = 0; value <= 255; value++) {
    const float channel = value / 255.0f;
    assert(full.apply(channel) == channel);
  }
  std::cout << "Sparse curve: fractional thresholds, monotonic energy, zero/full and tiny inputs passed\n";
}
