#include <array>
#include <cassert>
#include <iostream>
#include "ws2805_color_accessor.h"

using namespace esphome;
using ws2805::WS2805ColorAccessor;

void expect_color(Color color, uint8_t r, uint8_t g, uint8_t b) {
  assert(color.red == r && color.green == g && color.blue == b && color.white == 0);
}

int main() {
  const light::ESPColorCorrection correction;
  // RGB, GRB and BRG, each with both WW/CW orders as accepted by light.py.
  const std::array<std::array<uint8_t, 5>, 6> orders{{
      {{0, 1, 2, 3, 4}}, {{0, 1, 2, 4, 3}}, {{1, 0, 2, 3, 4}},
      {{1, 0, 2, 4, 3}}, {{1, 2, 0, 3, 4}}, {{1, 2, 0, 4, 3}},
  }};
  for (const auto &order : orders) {
    WS2805ColorAccessor accessor(correction);
    expect_color(accessor.get(0), 0, 0, 0);
    accessor.set(0, Color(1, 2, 3, 255));
    accessor.set_effect_data(0, 42);
    assert(accessor.get_effect_data(0) == 0);

    std::array<uint8_t, 15> pixels;
    pixels.fill(0xAA);
    std::array<uint8_t, 3> effects{{7, 8, 9}};
    accessor.set_buffers(pixels.data(), effects.data(), 3);
    accessor.set_channel_order(order[0], order[1], order[2]);
    // Every pixel has a five-byte stride. RGB writes must preserve both whites
    // and neighboring pixels, even when a color includes a white component.
    accessor.set_raw(1, Color(10, 20, 30, 255));
    assert(pixels[5 + order[0]] == 10 && pixels[5 + order[1]] == 20 && pixels[5 + order[2]] == 30);
    expect_color(accessor.get_raw(1), 10, 20, 30);
    accessor.set_channel_raw(1, COLOR_CHANNEL_W, 255);
    accessor.set_channel(1, COLOR_CHANNEL_W, 255);
    assert(accessor.get_channel_raw(1, COLOR_CHANNEL_W) == 0);
    assert(accessor.get_channel(1, COLOR_CHANNEL_W) == 0);

    accessor.set(1, Color(40, 50, 60, 255));
    expect_color(accessor.get(1), 40, 50, 60);
    expect_color(accessor.get_raw(1), 40 ^ 0x11, 50 ^ 0x22, 60 ^ 0x44);
    accessor.set_channel(1, COLOR_CHANNEL_G, 70);
    assert(accessor.get_channel(1, COLOR_CHANNEL_G) == 70);
    assert(accessor.get_channel_raw(1, COLOR_CHANNEL_G) == (70 ^ 0x22));

    accessor.set_effect_data(1, 91);
    assert(accessor.get_effect_data(1) == 91);
    assert(effects[0] == 7 && effects[2] == 9);
    for (int i = 0; i < 5; i++)
      assert(pixels[i] == 0xAA && pixels[10 + i] == 0xAA);
    assert(pixels[5 + order[3]] == 0xAA && pixels[5 + order[4]] == 0xAA);

    const auto before = pixels;
    for (int invalid : {-1, 3}) {
      accessor.set_raw(invalid, Color(255, 255, 255));
      accessor.set_effect_data(invalid, 255);
      expect_color(accessor.get_raw(invalid), 0, 0, 0);
      assert(accessor.get_effect_data(invalid) == 0);
    }
    accessor.set_channel_raw(1, 255, 255);
    assert(pixels == before);
#ifdef USE_LIGHT_PARTITION
    assert(!accessor.has_white_internal(1));
#ifdef USE_LIGHT_POWER_ESTIMATION
    assert(accessor.get_color_correction_internal(1) == &correction);
#endif
#endif
    // Existing views reference this accessor: detaching must make it harmless
    // before the backing buffers are released during cleanup or failed setup.
    accessor.set_buffers(nullptr, nullptr, 0);
    accessor.set_raw(1, Color(255, 255, 255));
    expect_color(accessor.get(1), 0, 0, 0);
    assert(pixels == before);
  }
  std::cout << "WS2805 accessor: all 6 channel orders, stride, whites, correction and lifecycle passed\n";
}
