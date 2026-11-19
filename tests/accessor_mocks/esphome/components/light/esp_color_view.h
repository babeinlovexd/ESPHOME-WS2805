#pragma once
#include <cstdint>

// Native test doubles for buffer behavior, not an ESPHome compatibility check.
// The firmware CI matrix compiles against the actual ESPHome headers.
namespace esphome {
constexpr uint8_t COLOR_CHANNEL_R = 0, COLOR_CHANNEL_G = 1, COLOR_CHANNEL_B = 2, COLOR_CHANNEL_W = 3;
struct Color {
  uint8_t red, green, blue, white;
  Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t w = 0) : red(r), green(g), blue(b), white(w) {}
};
namespace light {
class ESPColorCorrection {
 public:
  // Deliberately distinct, reversible channel transforms detect wrong routing
  // and accidentally applying correction to raw reads/writes.
  uint8_t color_correct_channel(uint8_t channel, uint8_t value) const {
    return value ^ static_cast<uint8_t>(0x11 << channel);
  }
  uint8_t color_uncorrect_channel(uint8_t channel, uint8_t value) const {
    return this->color_correct_channel(channel, value);
  }
  Color color_correct(Color color) const {
    return Color(this->color_correct_channel(0, color.red), this->color_correct_channel(1, color.green),
                 this->color_correct_channel(2, color.blue), this->color_correct_channel(3, color.white));
  }
};
class ESPColorAccessor {
 public:
  virtual Color get(int32_t) const = 0;
  virtual Color get_raw(int32_t) const = 0;
  virtual uint8_t get_channel(int32_t, uint8_t) const = 0;
  virtual uint8_t get_channel_raw(int32_t, uint8_t) const = 0;
  virtual uint8_t get_effect_data(int32_t) const = 0;
  virtual void set(int32_t, Color) = 0;
  virtual void set_raw(int32_t, Color) = 0;
  virtual void set_channel(int32_t, uint8_t, uint8_t) = 0;
  virtual void set_channel_raw(int32_t, uint8_t, uint8_t) = 0;
  virtual void set_effect_data(int32_t, uint8_t) = 0;
#ifdef USE_LIGHT_PARTITION
  virtual bool has_white_internal(int32_t) const { return true; }
#ifdef USE_LIGHT_POWER_ESTIMATION
  virtual const ESPColorCorrection *get_color_correction_internal(int32_t) const { return nullptr; }
#endif
#endif
};
}  // namespace light
}  // namespace esphome
