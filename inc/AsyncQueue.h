#pragma once
#include "LogRecord.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

enum class QueuePolicy {
    Block,      // Wait until space available
    Drop,       // Drop new message when full
    Overwrite   // Overwrite oldest message when full
};

// Thread-safe version of a classic circular queue (same logic as MyCircularQueue):
//   front_  ↔  front     (read index)
//   rear_   ↔  rear      (write index)
//   count_  ↔  count     (elements currently stored)
//   size_   ↔  size      (buffer capacity)
class AsyncLogQueue {
    std::vector<LogRecord> buffer_;
    size_t front_;
    size_t rear_;
    size_t count_;
    size_t size_;

    mutable std::mutex mtx_;
    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;

    QueuePolicy policy_;
    bool shutdown_;
    std::atomic<size_t> dropped_;

    bool isEmpty() const { return count_ == 0; }
    bool isFull()  const { return count_ == size_; }

public:
    explicit AsyncLogQueue(size_t k, QueuePolicy policy = QueuePolicy::Block)
        : buffer_(k),
          front_(0),
          rear_(0),
          count_(0),
          size_(k),
          policy_(policy),
          shutdown_(false),
          dropped_(0) {}

    AsyncLogQueue(const AsyncLogQueue&) = delete;
    AsyncLogQueue& operator=(const AsyncLogQueue&) = delete;

    // enQueue equivalent
    bool push(LogRecord value) {
        std::unique_lock<std::mutex> lock(mtx_);

        if (policy_ == QueuePolicy::Block) {
            cv_push_.wait(lock, [this] { return !isFull() || shutdown_; });
            if (shutdown_) return false;
        } else if (policy_ == QueuePolicy::Drop) {
            if (isFull()) {
                ++dropped_;
                return false;
            }
        } else if (policy_ == QueuePolicy::Overwrite) {
            if (isFull()) {
                front_ = (front_ + 1) % size_;
                --count_;
            }
        }

        buffer_[rear_] = std::move(value);
        rear_ = (rear_ + 1) % size_;
        ++count_;
        cv_pop_.notify_one();
        return true;
    }

    // deQueue equivalent (also returns the value via out)
    bool pop(LogRecord& out, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mtx_);

        if (!cv_pop_.wait_for(lock, timeout, [this] { return !isEmpty() || shutdown_; }))
            return false;
        if (isEmpty()) return false;

        out = std::move(buffer_[front_]);
        front_ = (front_ + 1) % size_;
        --count_;
        cv_push_.notify_one();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_;
    }

    size_t dropped() const { return dropped_.load(); }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mtx_);
        shutdown_ = true;
        cv_push_.notify_all();
        cv_pop_.notify_all();
    }

    bool is_shutdown() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return shutdown_;
    }
};
