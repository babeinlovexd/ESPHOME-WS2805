#include "ws2805_timing.h"

#include <cstdint>
#include <limits>

using esphome::ws2805::ns_to_rmt_ticks;
using esphome::ws2805::rmt_frame_timeout_ms;
using esphome::ws2805::split_rmt_reset;
using esphome::ws2805::us_to_rmt_ticks;
using esphome::ws2805::valid_rmt_pulse;

constexpr uint32_t modern_clock = 80000000;
constexpr uint32_t shared_clock = 40000000;
constexpr uint32_t legacy_clock = 10000000;

// The stock timings at the usual resolutions, including nearest-tick rounding.
static_assert(ns_to_rmt_ticks(300, modern_clock) == 24);
static_assert(ns_to_rmt_ticks(975, modern_clock) == 78);
static_assert(ns_to_rmt_ticks(800, modern_clock) == 64);
static_assert(us_to_rmt_ticks(300, modern_clock) == 24000);
static_assert(ns_to_rmt_ticks(300, shared_clock) == 12);
static_assert(ns_to_rmt_ticks(975, shared_clock) == 39);
static_assert(ns_to_rmt_ticks(800, shared_clock) == 32);
static_assert(us_to_rmt_ticks(300, shared_clock) == 12000);
static_assert(ns_to_rmt_ticks(300, legacy_clock) == 3);
static_assert(ns_to_rmt_ticks(975, legacy_clock) == 10);
static_assert(ns_to_rmt_ticks(800, legacy_clock) == 8);
static_assert(us_to_rmt_ticks(300, legacy_clock) == 3000);

// Zero cannot be used for a data pulse and 15-bit durations must not wrap.
static_assert(!valid_rmt_pulse(ns_to_rmt_ticks(6, modern_clock)));
static_assert(valid_rmt_pulse(ns_to_rmt_ticks(7, modern_clock)));
static_assert(!valid_rmt_pulse(ns_to_rmt_ticks(49, legacy_clock)));
static_assert(valid_rmt_pulse(ns_to_rmt_ticks(50, legacy_clock)));
static_assert(valid_rmt_pulse(ns_to_rmt_ticks(409593, modern_clock)));
static_assert(!valid_rmt_pulse(ns_to_rmt_ticks(409594, modern_clock)));
static_assert(ns_to_rmt_ticks(std::numeric_limits<uint32_t>::max(), modern_clock) == 343597384ULL);
static_assert(us_to_rmt_ticks(std::numeric_limits<uint32_t>::max(), modern_clock) == 343597383600ULL);

constexpr auto short_reset = split_rmt_reset(us_to_rmt_ticks(300, modern_clock));
static_assert(short_reset.first == 24000 && short_reset.second == 0 && !short_reset.clamped);
constexpr auto two_part_reset = split_rmt_reset(32768);
static_assert(two_part_reset.first == 32767 && two_part_reset.second == 1 && !two_part_reset.clamped);
constexpr auto max_reset = split_rmt_reset(65534);
static_assert(max_reset.first == 32767 && max_reset.second == 32767 && !max_reset.clamped);
constexpr auto excess_reset = split_rmt_reset(us_to_rmt_ticks(820, modern_clock));
static_assert(excess_reset.first == 32767 && excess_reset.second == 32767 && excess_reset.clamped);

// 1000 pixels take 64.3 ms at the stock worst case. Both frame length and reset
// must count, round the duration up, then add the 50 ms scheduling margin.
static_assert(rmt_frame_timeout_ms(modern_clock, 5000, 128, 24000) == 115);
static_assert(rmt_frame_timeout_ms(shared_clock, 5000, 64, 12000) == 115);
static_assert(rmt_frame_timeout_ms(legacy_clock, 5000, 16, 3000) == 115);
static_assert(rmt_frame_timeout_ms(modern_clock, 20000, 128, 24000) == 307);
static_assert(rmt_frame_timeout_ms(modern_clock, 5000, 256, 65534) == 179);
static_assert(rmt_frame_timeout_ms(1, 327675, 65534, 65534) == std::numeric_limits<int>::max());

int main() {}
