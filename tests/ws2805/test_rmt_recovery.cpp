#include <cassert>

#include "ws2805_light.h"

namespace {

using esphome::light::LightState;
using esphome::ws2805::WS2805LightOutput;
using mock_rmt::Operation;

constexpr int DIN_PIN = 4;
constexpr int FDIN_PIN = 5;

void expect_event(size_t index, Operation operation, int pin, int argument = -1) {
  const auto &event = mock_rmt::events.at(index);
  assert(event.operation == operation);
  assert(event.pin == pin);
  if (argument >= 0)
    assert(event.argument == argument);
}

void write_pending(WS2805LightOutput &output, LightState &state) {
  assert(state.pending_write);
  state.pending_write = false;
  output.write_state(&state);
}

void test_recovers_to_din_only() {
  WS2805LightOutput output(2, DIN_PIN);
  LightState state(&output);
  output.set_fdin_pin(FDIN_PIN);
  output.set_reset_pulse_us(550);
  output.setup_state(&state);
  output.setup();
  mock_rmt::reset();

  mock_rmt::fail_tx_pin = FDIN_PIN;
  const auto before = esphome::mock_micros;
  output.schedule_show();
  write_pending(output, state);

  assert(output.get_tx_error_count() == 1);
  assert(esphome::mock_micros - before >= 550);
  assert(mock_rmt::events.size() == 11);
  expect_event(0, Operation::WAIT, DIN_PIN);
  expect_event(1, Operation::WAIT, FDIN_PIN);
  expect_event(2, Operation::SYNC_RESET, -1);
  expect_event(3, Operation::TRANSMIT, DIN_PIN, 10);
  expect_event(4, Operation::TRANSMIT, FDIN_PIN, 10);
  expect_event(5, Operation::SYNC_DELETE, -1);
  expect_event(6, Operation::DISABLE, DIN_PIN);
  expect_event(7, Operation::WAIT, DIN_PIN, 0);
  expect_event(8, Operation::DISABLE, FDIN_PIN);
  expect_event(9, Operation::WAIT, FDIN_PIN, 0);
  expect_event(10, Operation::ENABLE, DIN_PIN);

  mock_rmt::events.clear();
  write_pending(output, state);
  assert(!state.pending_write);
  assert(mock_rmt::events.size() == 3);
  expect_event(0, Operation::WAIT, DIN_PIN);
  expect_event(1, Operation::TRANSMIT, DIN_PIN, 10);
  expect_event(2, Operation::WAIT, DIN_PIN);
  assert(output.get_tx_error_count() == 1);
}

void test_no_retry_when_drain_fails() {
  WS2805LightOutput output(2, DIN_PIN);
  LightState state(&output);
  output.set_fdin_pin(FDIN_PIN);
  output.setup_state(&state);
  output.setup();
  mock_rmt::reset();

  mock_rmt::fail_tx_pin = FDIN_PIN;
  mock_rmt::fail_zero_wait_pin = DIN_PIN;
  output.schedule_show();
  write_pending(output, state);

  assert(output.get_tx_error_count() == 1);
  assert(!state.pending_write);
  assert(mock_rmt::events.size() == 8);
  expect_event(5, Operation::SYNC_DELETE, -1);
  expect_event(6, Operation::DISABLE, DIN_PIN);
  expect_event(7, Operation::WAIT, DIN_PIN, 0);

  mock_rmt::events.clear();
  output.schedule_show();
  write_pending(output, state);
  assert(mock_rmt::events.empty());
}

}  // namespace

void test_rmt_recovery() {
  test_recovers_to_din_only();
  test_no_retry_when_drain_fails();
}
