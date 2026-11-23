#include "ws2805_light.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

namespace {

using esphome::light::LightState;
using esphome::ws2805::LedParams;
using esphome::ws2805::WS2805LightOutput;

void test_uses_realized_rmt_clock() {
  mock_rmt::reset();
  mock_rmt::actual_resolution_hz = 40'000'000;
  WS2805LightOutput output(4000, 4);
  LightState state(&output);
  output.set_fdin_pin(5);
  output.setup_state(&state);
  output.setup();

  assert(!output.failed);
  assert(mock_rmt::channels_created == 2);
  assert(output.get_rmt_resolution_hz() == 40'000'000);
  const auto &params = *static_cast<const LedParams *>(mock_rmt::simple_encoder.arg);
  assert(params.bit0.duration0 == 12 && params.bit0.duration1 == 39);
  assert(params.bit1.duration0 == 32 && params.bit1.duration1 == 32);
  assert(params.reset.duration0 == 12'000 && params.reset.duration1 == 0);
  assert(std::any_of(mock_rmt::events.begin(), mock_rmt::events.end(), [](const auto &event) {
    return event.operation == mock_rmt::Operation::WAIT && event.pin == 4 && event.argument == 307;
  }));
}

void expect_invalid_pulse(uint32_t nanoseconds, uint32_t actual_clock, int expected_channels) {
  mock_rmt::reset();
  mock_rmt::actual_resolution_hz = actual_clock;
  WS2805LightOutput output(10, 4);
  LightState state(&output);
  output.set_bit0_high_ns(nanoseconds);
  output.setup_state(&state);
  output.setup();

  assert(output.failed);
  assert(mock_rmt::channels_created == expected_channels);
  assert(mock_rmt::frame.empty());
}

}  // namespace

void test_rmt_timing() {
  test_uses_realized_rmt_clock();
  expect_invalid_pulse(1, 0, 0);
  expect_invalid_pulse(std::numeric_limits<uint32_t>::max(), 0, 0);
  // Ten nanoseconds fits one tick at the requested 80 MHz, but none at 40 MHz.
  expect_invalid_pulse(10, 40'000'000, 1);
}
