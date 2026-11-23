#include "ws2805_light.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

#include "esphome/components/partition/light_partition.h"

using esphome::light::ColorMode;
using esphome::light::LightColorValues;
using esphome::light::LightState;
using esphome::light::LightTransformer;
using esphome::ws2805::WS2805LightOutput;

void test_rmt_recovery();
void test_rmt_timing();

namespace {

constexpr ColorMode RGBWW = ColorMode::RGB_COLD_WARM_WHITE;
constexpr int RED = 1, BLUE = 2, WARM = 3, COLD = 4;

uint8_t pwm(int pixel, int channel) { return mock_rmt::frame.at(pixel * 5 + channel); }

bool is_black() {
  return std::all_of(mock_rmt::frame.begin(), mock_rmt::frame.end(), [](auto channel) { return channel == 0; });
}

LightColorValues color(ColorMode mode, float red, float blue, float cold, float warm, float brightness = 1.0f) {
  LightColorValues values;
  values.set_color_mode(mode);
  values.set_state(true);
  values.set_brightness(brightness);
  values.set_color_brightness(1.0f);
  values.set_red(red);
  values.set_green(0.0f);
  values.set_blue(blue);
  values.set_cold_white(cold);
  values.set_warm_white(warm);
  return values;
}

struct Strip {
  WS2805LightOutput output{3, 4};
  LightState state{&output};
  esphome::power_supply::PowerSupply supply;

  Strip() {
    mock_rmt::reset();
    esphome::power_supply::requested = false;
    esphome::mock_millis = 0;
    esphome::mock_micros = 0;
    this->output.setup_state(&this->state);
    this->output.set_power_supply(&this->supply);
    this->output.setup();
    assert(!this->output.failed);
    assert(mock_rmt::frame.size() == 15 && is_black());
  }

  void render(const LightColorValues &values) {
    this->state.current_values = values;
    this->output.update_state(&this->state);
    this->output.write_state(&this->state);
  }

  void apply(LightTransformer &transformer, uint32_t time_ms) {
    esphome::mock_millis = time_ms;
    esphome::mock_micros = 1000ULL * time_ms;
    this->state.transformer_active = true;
    auto colors = transformer.apply();
    assert(colors.has_value());
    this->render(*colors);
    this->state.transformer_active = false;
  }

  void set_effect(bool active) {
    this->state.effect_index = active ? 1 : 0;
    this->output.set_effect_active(active);
    if (active)
      this->output.clear_effect_data();
  }
};

void test_global_transition() {
  Strip strip;
  const auto red = color(RGBWW, 1, 0, 0, 0);
  const auto blue = color(RGBWW, 0, 1, 0, 0);
  const auto warm = color(RGBWW, 0, 0, 0, 1);
  strip.state.remote_values = blue;
  auto transformer = strip.output.create_default_transition();
  transformer->setup(red, blue, 8'000);
  strip.apply(*transformer, 4'000);
  assert(pwm(0, RED) >= 36 && pwm(0, RED) <= 38);
  assert(pwm(0, BLUE) == pwm(0, RED));
  strip.apply(*transformer, 8'000);
  assert(pwm(0, RED) == 0 && pwm(0, BLUE) == 255);
  strip.render(warm);
  assert(pwm(0, BLUE) == 0 && pwm(0, WARM) == 255);

  strip.output.set_color_interlock(true);
  const auto pure_red = color(ColorMode::RGB, 1, 0, 0, 0);
  const auto cold = color(ColorMode::COLD_WARM_WHITE, 1, 0, 1, 0);
  auto interlocked = strip.output.create_default_transition();
  esphome::mock_millis = 0;
  interlocked->setup(pure_red, cold, 8'000);
  strip.apply(*interlocked, 4'000);
  assert(is_black());
  strip.apply(*interlocked, 6'000);
  assert(pwm(0, RED) == 0 && pwm(0, COLD) >= 36 && pwm(0, COLD) <= 38);
}

void test_partition_and_white_power() {
  Strip strip;
  const auto warm = color(RGBWW, 0, 0, 0, 1);
  const auto cold = color(RGBWW, 0, 0, 1, 0);
  auto off = color(RGBWW, 0, 0, 0, 0);
  off.set_state(false);
  for (const auto &values : {warm, cold}) {
    strip.render(values);
    assert(mock_rmt::powered_at_last_tx);
    strip.render(off);
    assert(is_black() && !esphome::power_supply::requested);
  }
  assert(strip.supply.requests == 2 && strip.supply.releases == 2);

  using esphome::partition::AddressableSegment;
  using esphome::partition::PartitionLightOutput;
  PartitionLightOutput segment({AddressableSegment{&strip.state, 1, 2, false}});
  segment.get(0).set_red(54);
  segment.get(1).set_blue(84);
  segment.write_state(nullptr);
  strip.output.write_state(&strip.state);
  assert(pwm(0, RED) == 0 && pwm(1, RED) == 54 && pwm(2, BLUE) == 84);
}

void test_interlock_and_black_effect_frames() {
  Strip strip;
  strip.output.set_color_interlock(true);
  auto warm = color(ColorMode::COLD_WARM_WHITE, 0, 0, 0, 1);
  strip.state.remote_values = warm;
  strip.render(warm);
  assert(pwm(0, WARM) == 255);

  strip.set_effect(true);
  strip.render(warm);
  assert(is_black());
  strip.output.get(0).set_blue(255);
  strip.output.write_state(&strip.state);
  assert(pwm(0, BLUE) == 255 && pwm(0, WARM) == 0);
  strip.output.get(0).set_blue(0);
  strip.output.write_state(&strip.state);
  assert(is_black());
  strip.set_effect(false);
  strip.render(warm);
  assert(pwm(0, WARM) == 255);
  assert(strip.state.remote_values.get_warm_white() == 1.0f);
  assert(strip.state.publications == 0);

  // Without interlock, an effect can mix RGB and global CWWW.
  strip.output.set_color_interlock(false);
  warm.set_color_mode(RGBWW);
  strip.set_effect(true);
  strip.render(warm);
  strip.output.get(0).set_blue(255);
  strip.output.write_state(&strip.state);
  assert(pwm(0, BLUE) == 255 && pwm(0, WARM) == 255 && pwm(1, WARM) == 255);
}

void test_suppression_resets_dither() {
  Strip strip;
  strip.output.set_dithering(true);
  strip.state.transformer_active = true;
  // This request alternates between 0 and 1 without interlock.
  const float brightness = std::pow(0.49f / 255.0f, 1.0f / 2.8f);
  const auto dim = color(RGBWW, 0, 0, 0, 1, brightness);
  strip.render(dim);
  assert(pwm(0, WARM) == 0);
  strip.output.set_color_interlock(true);
  strip.set_effect(true);
  strip.render(dim);
  assert(is_black());
  strip.output.set_color_interlock(false);
  strip.render(dim);
  assert(pwm(0, WARM) == 0);
  strip.output.write_state(&strip.state);
  assert(pwm(0, WARM) == 1);
}

void test_sparse_color_limits() {
  Strip strip;
  strip.output.set_sparse_dimming(true);
  strip.output.set_correction(0.5f, 0.25f, 0.75f);
  auto mix = color(RGBWW, 1, 1, 0, 1);
  mix.set_green(1);
  strip.render(mix);
  for (int pixel = 0; pixel < 3; ++pixel) {
    assert(pwm(pixel, RED) == 128 && pwm(pixel, 0) == 64 && pwm(pixel, BLUE) == 191);
    assert(pwm(pixel, WARM) == 255);
  }
  mix.set_brightness(0.25f / 255.0f);
  strip.render(mix);
  for (const int dark_pixel : {0, 2})
    assert(pwm(dark_pixel, RED) == 0 && pwm(dark_pixel, WARM) == 0);
  assert(pwm(1, RED) == 1 && pwm(1, BLUE) == 1 && pwm(1, 0) == 0 && pwm(1, WARM) == 1);

  // Normal rendering still applies that correction through ESPHome gamma.
  strip.output.set_sparse_dimming(false);
  strip.render(color(RGBWW, 1, 0, 0, 0));
  assert(pwm(0, RED) >= 36 && pwm(0, RED) <= 38);
}

void test_simple_encoder_callback() {
  Strip strip;
  const auto &encoder = mock_rmt::simple_encoder;
  const uint8_t byte = 0xA5;
  std::array<rmt_symbol_word_t, 9> symbols{};
  bool done = false;
  assert(encoder.callback(&byte, 1, 0, 7, symbols.data(), &done, encoder.arg) == 0 && !done);

  size_t count = 0;
  while (!done) {
    auto written = encoder.callback(&byte, 1, count, symbols.size() - count, symbols.data() + count, &done, encoder.arg);
    assert(written > 0 && written <= symbols.size() - count);
    count += written;
  }
  assert(count == symbols.size());
  for (size_t bit = 0; bit < 8; ++bit) {
    const bool is_one = (byte & (0x80 >> bit)) != 0;
    assert(symbols[bit].level0 == 1 && symbols[bit].level1 == 0);
    assert(symbols[bit].duration0 == (is_one ? 64 : 24));
    assert(symbols[bit].duration1 == (is_one ? 64 : 78));
  }
  assert(symbols.back().level0 == 0 && symbols.back().level1 == 0);
  assert(symbols.back().duration0 + symbols.back().duration1 == 24'000);
}

}  // namespace

int main() {
  test_global_transition();
  test_partition_and_white_power();
  test_interlock_and_black_effect_frames();
  test_suppression_resets_dither();
  test_sparse_color_limits();
  test_simple_encoder_callback();
  test_rmt_recovery();
  test_rmt_timing();
  std::cout << "WS2805 renderer and RMT regression tests passed\n";
}
