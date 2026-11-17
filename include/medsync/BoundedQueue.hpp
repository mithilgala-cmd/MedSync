#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace medsync {

// Fixed-capacity FIFO queue shared between threads.
//
// Design choice: producers never block. tryPush() fails when the queue is full
// or closed, so the console thread can report backpressure instead of freezing.
// Consumers block in pop() and are woken when an item arrives or the queue is
// closed. Because producers do not wait for space, a second condition variable
// for "not full" would never be used.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedQueue capacity must be positive");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    // Returns false when the queue is full or closed. Never blocks.
    bool tryPush(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_ || items_.size() >= capacity_) {
                return false;
            }
            items_.push_back(std::move(value));
        }
        notEmpty_.notify_one();
        return true;
    }

    // Blocks until an item is available, or until the queue is closed and
    // drained. Returns false in the closed-and-empty case.
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] { return closed_ || !items_.empty(); });
        if (items_.empty()) {
            return false;
        }
        out = std::move(items_.front());
        items_.pop_front();
        return true;
    }

    // Bounded wait, used by tests and components that must not hang forever.
    // Returns false on timeout or when the queue is closed and empty.
    bool popFor(T& out, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!notEmpty_.wait_for(lock, timeout,
                                [this] { return closed_ || !items_.empty(); })) {
            return false;
        }
        if (items_.empty()) {
            return false;
        }
        out = std::move(items_.front());
        items_.pop_front();
        return true;
    }

    // Discards queued items but leaves the queue open. Used to cancel work that
    // has not started yet (for example after an emergency stop).
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
    }

    // Wakes every blocked consumer. Idempotent.
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

    std::size_t capacity() const noexcept {
        return capacity_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::deque<T> items_;
    const std::size_t capacity_;
    bool closed_{false};
};

} // namespace medsync
