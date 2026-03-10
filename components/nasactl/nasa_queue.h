#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

#include "esphome/core/log.h"

namespace nasactl {

// Size-capped queue that silently drops items when full
template<typename T>
class LimitedQueue {
 public:
  explicit LimitedQueue(size_t max_size) : max_size_(max_size) {}

  bool push(const T &item) {
    if (queue_.size() >= max_size_) {
      ESP_LOGW("nasactl.queue", "Queue full (%zu), dropping item", max_size_);
      return false;
    }
    queue_.push(item);
    return true;
  }

  T pop() {
    if (queue_.empty())
      return T{};
    T item = queue_.front();
    queue_.pop();
    return item;
  }

  bool empty() const { return queue_.empty(); }
  size_t size() const { return queue_.size(); }

 private:
  std::queue<T> queue_;
  size_t max_size_;
};

// Batches items and dispatches them in groups.
// Dispatches when batch_size items accumulate OR delay_ms elapses, whichever first.
template<typename T>
class BatchDispatcher {
 public:
  BatchDispatcher(size_t queue_capacity, size_t batch_size, uint32_t delay_ms)
      : queue_(queue_capacity), batch_size_(batch_size), delay_ms_(delay_ms) {}

  void set_callback(std::function<void(std::vector<T> &)> callback) {
    callback_ = callback;
  }

  void push(const T &item) {
    queue_.push(item);
  }

  void push(const std::vector<T> &items) {
    for (const auto &item : items) {
      queue_.push(item);
    }
  }

  // Call this from loop(). Dispatches when ready.
  void update(uint32_t now_ms) {
    if (queue_.empty())
      return;

    // Transfer from queue to pending batch
    while (!queue_.empty() && pending_.size() < batch_size_) {
      pending_.push_back(queue_.pop());
    }

    bool batch_full = pending_.size() >= batch_size_;
    bool delay_elapsed = (now_ms - last_dispatch_ms_) >= delay_ms_;

    if (!pending_.empty() && (batch_full || delay_elapsed)) {
      if (callback_) {
        callback_(pending_);
      }
      pending_.clear();
      last_dispatch_ms_ = now_ms;
    }
  }

 private:
  LimitedQueue<T> queue_;
  std::vector<T> pending_;
  std::function<void(std::vector<T> &)> callback_;
  size_t batch_size_;
  uint32_t delay_ms_;
  uint32_t last_dispatch_ms_{0};
};

}  // namespace nasactl
