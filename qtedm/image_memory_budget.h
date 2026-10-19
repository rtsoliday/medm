#pragma once

#include <cstdint>
#include <memory>
#include <mutex>

/* Reservations follow shared frame ownership, including queued completions.
 * Destroying a widget cannot return its budget while a worker still uses it. */
class ImageMemoryBudget
{
public:
  explicit ImageMemoryBudget(uint64_t maximumBytes)
    : state_(std::make_shared<State>(maximumBytes))
  {
  }

  std::shared_ptr<void> reserve(uint64_t bytes)
  {
    const auto state = state_;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      if (bytes == 0 || bytes > state->maximum - state->used) {
        return {};
      }
      state->used += bytes;
    }
    /* shared_ptr invokes the deleter if its control-block allocation fails;
     * construct it outside the lock so that rollback cannot deadlock. */
    return std::shared_ptr<void>(state.get(), [state, bytes](void *) {
      std::lock_guard<std::mutex> releaseLock(state->mutex);
      state->used -= bytes;
    });
  }

  uint64_t bytesInUse() const
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->used;
  }

private:
  struct State
  {
    explicit State(uint64_t bytes) : maximum(bytes) {}
    std::mutex mutex;
    const uint64_t maximum;
    uint64_t used = 0;
  };
  std::shared_ptr<State> state_;
};
