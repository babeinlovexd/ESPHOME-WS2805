#pragma once

#include <cstdint>
#include <limits>

namespace esphome {
namespace ws2805 {

constexpr uint16_t RMT_MAX_DURATION_TICKS = 32767;

constexpr uint64_t ns_to_rmt_ticks(uint32_t ns, uint32_t clock_hz) {
  return (uint64_t(ns) * clock_hz + 500000000ULL) / 1000000000ULL;
}

constexpr uint64_t us_to_rmt_ticks(uint32_t us, uint32_t clock_hz) {
  return (uint64_t(us) * clock_hz + 500000ULL) / 1000000ULL;
}

constexpr bool valid_rmt_pulse(uint64_t ticks) { return ticks > 0 && ticks <= RMT_MAX_DURATION_TICKS; }

struct RmtResetTicks {
  uint16_t first;
  uint16_t second;
  bool clamped;
};

constexpr RmtResetTicks split_rmt_reset(uint64_t ticks) {
  constexpr uint32_t max_reset_ticks = 2U * RMT_MAX_DURATION_TICKS;
  const bool clamped = ticks > max_reset_ticks;
  if (clamped)
    ticks = max_reset_ticks;
  const auto first = static_cast<uint16_t>(ticks > RMT_MAX_DURATION_TICKS ? RMT_MAX_DURATION_TICKS : ticks);
  return {first, static_cast<uint16_t>(ticks - first), clamped};
}

constexpr int rmt_frame_timeout_ms(uint32_t clock_hz, uint32_t frame_bytes, uint16_t longest_bit_ticks,
                                   uint32_t reset_ticks) {
  constexpr int margin_ms = 50;
  constexpr uint64_t max_wait_ms = std::numeric_limits<int>::max();
  if (clock_hz == 0)
    return margin_ms;
  const uint64_t frame_ticks = uint64_t(frame_bytes) * 8 * longest_bit_ticks + reset_ticks;
  const uint64_t wait_ms = (frame_ticks * 1000 + clock_hz - 1) / clock_hz + margin_ms;
  return static_cast<int>(wait_ms > max_wait_ms ? max_wait_ms : wait_ms);
}

}  // namespace ws2805
}  // namespace esphome
