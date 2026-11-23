#pragma once

#include "esphome_runtime.h"

#include <cstdlib>

#define ESP_IDF_VERSION_VAL(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5, 5, 6)
#define SOC_RMT_SUPPORT_DMA 1
#define SOC_RMT_SUPPORT_ASYNC_STOP 1
#define SOC_RMT_MEM_WORDS_PER_CHANNEL 48
#define MALLOC_CAP_8BIT 1
#define MALLOC_CAP_INTERNAL 2

using esp_err_t = int;
using gpio_num_t = int;
using soc_module_clk_t = int;
inline constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1;
inline constexpr esp_err_t ESP_ERR_NOT_FOUND = 0x105, ESP_ERR_INVALID_STATE = 0x103, ESP_ERR_TIMEOUT = 0x107;
inline constexpr int RMT_CLK_SRC_DEFAULT = 0, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED = 0;

inline void *heap_caps_malloc(size_t size, unsigned) { return std::malloc(size); }
inline void heap_caps_free(void *pointer) { std::free(pointer); }
inline int64_t esp_timer_get_time() { return static_cast<int64_t>(esphome::mock_micros); }
inline esp_err_t esp_clk_tree_src_get_freq_hz(int, int, uint32_t *frequency) {
  *frequency = 80'000'000;
  return ESP_OK;
}

struct rmt_symbol_word_t {
  uint32_t duration0 : 15, level0 : 1, duration1 : 15, level1 : 1;
};

struct rmt_host_sync;

struct rmt_host_channel {
  int pin;
  uint32_t resolution_hz;
  bool queued{false};
  bool canceled{false};
  rmt_host_sync *sync{nullptr};
};

struct rmt_host_sync {
  std::vector<rmt_host_channel *> channels;
};

using rmt_channel_handle_t = rmt_host_channel *;
using rmt_encoder_handle_t = const void *;
using rmt_sync_manager_handle_t = rmt_host_sync *;

struct rmt_tx_channel_config_t {
  int clk_src;
  uint32_t resolution_hz;
  gpio_num_t gpio_num;
  uint32_t trans_queue_depth;
  size_t mem_block_symbols;
  int intr_priority;
  struct {
    bool invert_out, with_dma;
  } flags;
};

struct rmt_sync_manager_config_t {
  rmt_channel_handle_t *tx_channel_array;
  size_t array_size;
};

struct rmt_simple_encoder_config_t {
  size_t (*callback)(const void *, size_t, size_t, size_t, rmt_symbol_word_t *, bool *, void *);
  void *arg;
  size_t min_chunk_size;
};

struct rmt_copy_encoder_config_t {};
struct rmt_transmit_config_t {};

namespace mock_rmt {

enum class Operation { TRANSMIT, WAIT, ENABLE, DISABLE, SYNC_DELETE, SYNC_RESET, SYNC_CREATE };

struct Event {
  Operation operation;
  int pin;
  int argument;
};

inline std::vector<Event> events;
inline int fail_tx_pin{-1};
inline int fail_wait_pin{-1};
inline int fail_zero_wait_pin{-1};
inline unsigned channels_created{0};
inline uint32_t actual_resolution_hz{0};
inline bool powered_at_last_tx{false};
inline std::vector<uint8_t> frame;
inline rmt_simple_encoder_config_t simple_encoder{};

inline void reset() {
  events.clear();
  frame.clear();
  fail_tx_pin = fail_wait_pin = fail_zero_wait_pin = -1;
  channels_created = 0;
  actual_resolution_hz = 0;
  powered_at_last_tx = false;
}

inline bool take_failure(int &failure_pin, int pin) {
  if (failure_pin != pin)
    return false;
  failure_pin = -1;
  return true;
}

}  // namespace mock_rmt

inline esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t *config, rmt_channel_handle_t *result) {
  ++mock_rmt::channels_created;
  auto resolution = mock_rmt::actual_resolution_hz ? mock_rmt::actual_resolution_hz : config->resolution_hz;
  *result = new rmt_host_channel{config->gpio_num, resolution};
  return ESP_OK;
}
inline esp_err_t rmt_get_channel_resolution(rmt_channel_handle_t channel, uint32_t *frequency) {
  *frequency = channel->resolution_hz;
  return ESP_OK;
}
inline esp_err_t rmt_new_copy_encoder(const rmt_copy_encoder_config_t *, rmt_encoder_handle_t *result) {
  *result = &mock_rmt::simple_encoder;
  return ESP_OK;
}
inline esp_err_t rmt_new_simple_encoder(const rmt_simple_encoder_config_t *config, rmt_encoder_handle_t *result) {
  mock_rmt::simple_encoder = *config;
  *result = &mock_rmt::simple_encoder;
  return ESP_OK;
}
inline esp_err_t rmt_new_sync_manager(const rmt_sync_manager_config_t *config, rmt_sync_manager_handle_t *result) {
  mock_rmt::events.push_back({mock_rmt::Operation::SYNC_CREATE, -1, 0});
  auto *first = config->tx_channel_array;
  *result = new rmt_host_sync{{first, first + config->array_size}};
  for (auto *channel : (*result)->channels)
    channel->sync = *result;
  return ESP_OK;
}
inline esp_err_t rmt_del_sync_manager(rmt_sync_manager_handle_t manager) {
  mock_rmt::events.push_back({mock_rmt::Operation::SYNC_DELETE, -1, 0});
  for (auto *channel : manager->channels)
    channel->sync = nullptr;
  delete manager;
  return ESP_OK;
}
inline esp_err_t rmt_sync_reset(rmt_sync_manager_handle_t) {
  mock_rmt::events.push_back({mock_rmt::Operation::SYNC_RESET, -1, 0});
  return ESP_OK;
}
inline esp_err_t rmt_enable(rmt_channel_handle_t channel) {
  mock_rmt::events.push_back({mock_rmt::Operation::ENABLE, channel->pin, 0});
  return ESP_OK;
}
inline esp_err_t rmt_disable(rmt_channel_handle_t channel) {
  mock_rmt::events.push_back({mock_rmt::Operation::DISABLE, channel->pin, 0});
  // A canceled descriptor still has to be collected by wait_all_done.
  channel->canceled |= channel->queued;
  channel->queued = false;
  return ESP_OK;
}
inline esp_err_t rmt_del_channel(rmt_channel_handle_t channel) {
  delete channel;
  return ESP_OK;
}
inline esp_err_t rmt_del_encoder(rmt_encoder_handle_t) { return ESP_OK; }

inline esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t channel, int timeout_ms) {
  mock_rmt::events.push_back({mock_rmt::Operation::WAIT, channel->pin, timeout_ms});
  if (mock_rmt::take_failure(mock_rmt::fail_wait_pin, channel->pin) ||
      (timeout_ms == 0 && mock_rmt::take_failure(mock_rmt::fail_zero_wait_pin, channel->pin)) || channel->queued) {
    return ESP_ERR_TIMEOUT;
  }
  channel->canceled = false;
  return ESP_OK;
}

inline esp_err_t rmt_transmit(rmt_channel_handle_t channel, rmt_encoder_handle_t, const void *bytes, size_t size,
                             const rmt_transmit_config_t *) {
  mock_rmt::events.push_back({mock_rmt::Operation::TRANSMIT, channel->pin, static_cast<int>(size)});
  if (mock_rmt::take_failure(mock_rmt::fail_tx_pin, channel->pin))
    return ESP_FAIL;
  if (channel->queued || channel->canceled)
    return ESP_ERR_INVALID_STATE;
  if (channel->sync != nullptr) {
    auto &group = channel->sync->channels;
    channel->queued = true;
    if (std::all_of(group.begin(), group.end(), [](auto *member) { return member->queued; })) {
      for (auto *member : group)
        member->queued = false;
    }
  }
  const auto *first = static_cast<const uint8_t *>(bytes);
  mock_rmt::frame.assign(first, first + size);
  mock_rmt::powered_at_last_tx = esphome::power_supply::requested;
  return ESP_OK;
}
