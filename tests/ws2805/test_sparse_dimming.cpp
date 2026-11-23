#include "ws2805_sparse_dimming.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using esphome::ws2805::make_sparse_dimming_plan;
using esphome::ws2805::sparse_first_pixel;
using esphome::ws2805::SparseDimmingPlan;

static int pwm(const SparseDimmingPlan &plan, float channel) {
  return static_cast<int>(std::lround(plan.apply_unscaled(channel) * 255.0f));
}

static std::vector<int> selected_pixels(int count, uint8_t step) {
  std::vector<int> pixels;
  for (int index = sparse_first_pixel(count, step); index < count; index += step)
    pixels.push_back(index);
  return pixels;
}

int main() {
  assert(pwm(make_sparse_dimming_plan(0.0f), 1.0f) == 0);
  assert(pwm(make_sparse_dimming_plan(-0.25f), 1.0f) == 0);
  const auto dimmest = make_sparse_dimming_plan(std::numeric_limits<float>::min());
  assert(dimmest.step == 8);
  assert(pwm(dimmest, 1.0f) == 1);

  // Both requests must emit the same reference. The original proposal
  // rounded full red to zero at the first request and to two at the second.
  assert(pwm(make_sparse_dimming_plan(0.25f / 255.0f), 1.0f) == 1);
  assert(pwm(make_sparse_dimming_plan(0.51f / 255.0f), 1.0f) == 1);
  // Preserve weak channels until final quantization. At this reference,
  // equal white shares round to one, a smaller contribution to zero.
  assert(pwm(dimmest, 0.0f) == 0);
  assert(pwm(dimmest, 0.30f) == 0);
  assert(pwm(dimmest, 0.50f) == 1);

  const float join = 8.0f / 255.0f;
  for (float brightness : {std::nextafter(join, 0.0f), join, std::nextafter(join, 1.0f), 8.1f / 255.0f}) {
    const auto plan = make_sparse_dimming_plan(brightness);
    assert(plan.step == 1);
    assert(pwm(plan, 1.0f) == 1);
  }

  const auto full = make_sparse_dimming_plan(1.0f);
  assert(full.step == 1);
  for (float channel : {0.0f, 0.30f, 0.50f, 0.75f, 1.0f})
    assert(full.apply_unscaled(channel) == channel);

  assert((selected_pixels(24, 8) == std::vector<int>{3, 11, 19}));
  assert((selected_pixels(24, 7) == std::vector<int>{1, 8, 15, 22}));
  assert((selected_pixels(1, 8) == std::vector<int>{0}));
  assert((selected_pixels(2, 8) == std::vector<int>{0}));
  assert(selected_pixels(0, 8).empty());

  // The total digital output for a fixed channel ratio cannot drop as
  // brightness increases, even though different pixels can be selected.
  constexpr std::array<float, 8> channels{0.0f, 0.1f, 0.25f, 0.3f, 0.5f, 0.75f, 0.95f, 1.0f};
  for (int count : {1, 2, 7, 8, 9, 24, 43, 1000, 65535}) {
    std::array<int, channels.size()> previous{};
    for (int sample = 0; sample <= 25500; sample++) {
      const auto plan = make_sparse_dimming_plan(static_cast<float>(sample) / 25500.0f);
      const int selected = (count - 1) / plan.step + 1;
      for (std::size_t channel = 0; channel < channels.size(); channel++) {
        const int total = selected * pwm(plan, channels[channel]);
        assert(total >= previous[channel]);
        previous[channel] = total;
      }
    }
  }
}
