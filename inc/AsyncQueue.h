#pragma once
#include "LogRecord.h"
#include <vector>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>


enum class QueuePolicy {
    Block,      // Wait until space available
    Drop,       // Drop new message when full
    Overwrite   // Overwrite oldest message when full
};

class AsyncLogQueue {
    std::vector<std::optional<LogRecord>> buffer_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;
    mutable std::mutex mtx_;
    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;
    QueuePolicy policy_;
    bool shutdown_ = false;
    std::atomic<size_t> dropped_{0};

public:
    explicit AsyncLogQueue(size_t max_size, QueuePolicy policy = QueuePolicy::Block)
        : buffer_(max_size), policy_(policy) {}

    AsyncLogQueue(const AsyncLogQueue&) = delete;
    AsyncLogQueue& operator=(const AsyncLogQueue&) = delete;

    bool push(LogRecord record) {
        std::unique_lock lock(mtx_);
        if (policy_ == QueuePolicy::Block) {
            cv_push_.wait(lock, [this] { return size_ < buffer_.size() || shutdown_; });
            if (shutdown_) return false;
        } else if (policy_ == QueuePolicy::Drop) {
            if (size_ >= buffer_.size()) {
                ++dropped_;
                return false;
            }
        } else if (policy_ == QueuePolicy::Overwrite) {
            if (size_ >= buffer_.size()) {
                buffer_[head_] = std::nullopt;
                head_ = (head_ + 1) % buffer_.size();
                --size_;
            }
        }

        buffer_[tail_] = std::move(record);
        tail_ = (tail_ + 1) % buffer_.size();
        ++size_;
        cv_pop_.notify_one();
        return true;
    }

    std::optional<LogRecord> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock lock(mtx_);
        if (!cv_pop_.wait_for(lock, timeout, [this] { return size_ > 0 || shutdown_; }))
            return std::nullopt;
        if (size_ == 0) return std::nullopt;

        auto record = std::move(*buffer_[head_]);
        buffer_[head_] = std::nullopt;
        head_ = (head_ + 1) % buffer_.size();
        --size_;
        cv_push_.notify_one();
        return record;
    }

    size_t size() const {
        std::lock_guard lock(mtx_);
        return size_;
    }

    size_t dropped() const { return dropped_.load(); }

    void shutdown() {
        std::lock_guard lock(mtx_);
        shutdown_ = true;
        cv_push_.notify_all();
        cv_pop_.notify_all();
    }

    bool is_shutdown() const {
        std::lock_guard lock(mtx_);
        return shutdown_;
    }
};
