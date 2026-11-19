#pragma once

#include "esphome/core/version.h"

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
#include "esphome/components/light/esp_color_view.h"

namespace esphome::ws2805 {

// Effects expose RGB only. ChannelColors/BufferColorAccessor cannot describe
// WS2805's five-byte stride, and neither global white channel belongs to an
// addressable effect's color view.
class WS2805ColorAccessor final : public light::ESPColorAccessor {
 public:
  explicit WS2805ColorAccessor(const light::ESPColorCorrection &correction) : correction_(correction) {}

  void set_buffers(uint8_t *color_data, uint8_t *effect_data, uint16_t num_leds) {
    this->color_data_ = color_data;
    this->effect_data_ = effect_data;
    this->num_leds_ = num_leds;
  }
  void set_channel_order(uint8_t r, uint8_t g, uint8_t b) {
    this->offsets_[0] = r;
    this->offsets_[1] = g;
    this->offsets_[2] = b;
  }

  Color get(int32_t index) const override {
    if (!this->valid_index_(index) || this->color_data_ == nullptr)
      return Color();
    return Color(this->get_channel(index, COLOR_CHANNEL_R), this->get_channel(index, COLOR_CHANNEL_G),
                 this->get_channel(index, COLOR_CHANNEL_B));
  }
  Color get_raw(int32_t index) const override {
    return Color(this->get_channel_raw(index, COLOR_CHANNEL_R), this->get_channel_raw(index, COLOR_CHANNEL_G),
                 this->get_channel_raw(index, COLOR_CHANNEL_B));
  }
  uint8_t get_channel(int32_t index, uint8_t channel) const override {
    const uint8_t *value = this->channel_(index, channel);
    return value == nullptr ? 0 : this->correction_.color_uncorrect_channel(channel, *value);
  }
  uint8_t get_channel_raw(int32_t index, uint8_t channel) const override {
    const uint8_t *value = this->channel_(index, channel);
    return value == nullptr ? 0 : *value;
  }
  uint8_t get_effect_data(int32_t index) const override {
    return this->valid_index_(index) && this->effect_data_ != nullptr ? this->effect_data_[index] : 0;
  }

  void set(int32_t index, Color color) override {
    // An unsupported white value must not influence RGB dark-pixel correction.
    color.white = 0;
    this->set_raw(index, this->correction_.color_correct(color));
  }
  void set_raw(int32_t index, Color color) override {
    this->set_channel_raw(index, COLOR_CHANNEL_R, color.red);
    this->set_channel_raw(index, COLOR_CHANNEL_G, color.green);
    this->set_channel_raw(index, COLOR_CHANNEL_B, color.blue);
  }
  void set_channel(int32_t index, uint8_t channel, uint8_t value) override {
    uint8_t *output = this->channel_(index, channel);
    if (output != nullptr)
      *output = this->correction_.color_correct_channel(channel, value);
  }
  void set_channel_raw(int32_t index, uint8_t channel, uint8_t value) override {
    uint8_t *output = this->channel_(index, channel);
    if (output != nullptr)
      *output = value;
  }
  void set_effect_data(int32_t index, uint8_t value) override {
    if (this->valid_index_(index) && this->effect_data_ != nullptr)
      this->effect_data_[index] = value;
  }

#ifdef USE_LIGHT_PARTITION
  bool has_white_internal(int32_t) const override { return false; }
#ifdef USE_LIGHT_POWER_ESTIMATION
  const light::ESPColorCorrection *get_color_correction_internal(int32_t) const override {
    return &this->correction_;
  }
#endif
#endif

 protected:
  bool valid_index_(int32_t index) const { return index >= 0 && index < this->num_leds_; }
  uint8_t *channel_(int32_t index, uint8_t channel) const {
    if (!this->valid_index_(index) || this->color_data_ == nullptr || channel > COLOR_CHANNEL_B)
      return nullptr;
    return this->color_data_ + 5 * index + this->offsets_[channel];
  }

  const light::ESPColorCorrection &correction_;
  uint8_t *color_data_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint16_t num_leds_{0};
  uint8_t offsets_[3]{1, 0, 2};
};

}  // namespace esphome::ws2805
#endif
