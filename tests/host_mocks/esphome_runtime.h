#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#define ESPHOME_ALWAYS_INLINE
#define ESPDEPRECATED(...)
#define ESPHOME_LOG_TAG(var, value)
#define ESP_LOGCONFIG(...)
#define ESP_LOGD(...)
#define ESP_LOGE(...)
#define ESP_LOGW(...)
#define USE_LIGHT_GAMMA_LUT
#define USE_LIGHT_PARTITION
#define USE_POWER_SUPPLY
#define VERSION_CODE(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#define PROGMEM
#define IRAM_ATTR
#define HOT

#ifdef _MSC_VER
#define __attribute__(...)
#define __builtin_is_constant_evaluated() std::is_constant_evaluated()
#define __builtin_ctz(value) std::countr_zero(value)
#define __builtin_ctzl(value) std::countr_zero(value)
#define __builtin_ctzll(value) std::countr_zero(value)
#endif

inline uint8_t progmem_read_byte(const uint8_t *value) { return *value; }
inline uint16_t progmem_read_uint16(const uint16_t *value) { return *value; }

namespace esphome {

using std::clamp;
using std::make_unique;
using std::optional;

inline uint32_t mock_millis{0};
inline uint64_t mock_micros{0};
inline uint32_t millis() { return mock_millis; }
inline uint32_t micros() { return static_cast<uint32_t>(mock_micros); }
inline void delay_microseconds_safe(uint32_t duration_us) {
  mock_micros += duration_us;
  mock_millis = static_cast<uint32_t>(mock_micros / 1000);
}
inline uint32_t random_uint32() { return 0; }

template<typename T> struct RAMAllocator {
  T *allocate(size_t size) { return new T[size]; }
};

namespace setup_priority {
inline constexpr float HARDWARE = 800.0f;
}

class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void call_setup() { this->setup(); }
  virtual float get_setup_priority() const { return 0; }
  void mark_failed() { this->failed = true; }
  void status_set_warning() {}
  void status_clear_warning() {}

  bool failed{false};
};

namespace power_supply {

inline bool requested{false};

class PowerSupply {
 public:
  unsigned requests{0};
  unsigned releases{0};
};

class PowerSupplyRequester {
 public:
  void set_parent(PowerSupply *supply) { this->supply_ = supply; }
  void request() {
    if (this->supply_ != nullptr && !this->requested_) {
      this->requested_ = requested = true;
      ++this->supply_->requests;
    }
  }
  void unrequest() {
    if (this->supply_ != nullptr && this->requested_) {
      this->requested_ = requested = false;
      ++this->supply_->releases;
    }
  }

 private:
  PowerSupply *supply_{nullptr};
  bool requested_{false};
};

}  // namespace power_supply
}  // namespace esphome
