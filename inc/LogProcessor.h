#pragma once
#include "LogSink.h"
#include "AsyncQueue.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

class LogProcessor {
    std::vector<std::shared_ptr<ILogSink>> sinks_;
    AsyncLogQueue* queue_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::chrono::milliseconds flush_interval_;

public:
    LogProcessor(std::vector<std::shared_ptr<ILogSink>> sinks,
                 AsyncLogQueue* queue,
                 std::chrono::milliseconds flush_interval = std::chrono::milliseconds(1000))
        : sinks_(std::move(sinks)), queue_(queue), flush_interval_(flush_interval) {}

    ~LogProcessor() { stop(); }

    LogProcessor(const LogProcessor&) = delete;
    LogProcessor& operator=(const LogProcessor&) = delete;
    LogProcessor(LogProcessor&&) = delete;
    LogProcessor& operator=(LogProcessor&&) = delete;

    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) return;
        thread_ = std::thread(&LogProcessor::process, this);
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (queue_) queue_->shutdown();
        if (thread_.joinable()) thread_.join();
        flush_all();
    }

private:
    void process() {
        auto last_flush = std::chrono::steady_clock::now();

        while (running_ || queue_->size() > 0) {
            LogRecord record;
            if (queue_->pop(record, std::chrono::milliseconds(10)))
                dispatch(record);

            auto now = std::chrono::steady_clock::now();
            if (now - last_flush >= flush_interval_) {
                flush_all();
                last_flush = now;
            }
        }
    }

    void dispatch(const LogRecord& record) {
        for (auto& sink : sinks_) {
            if (!sink) continue;
            try {
                sink->write(record);
            } catch (...) {
                // Production note: increment a failure counter or write to stderr
            }
        }
    }

    void flush_all() {
        for (auto& sink : sinks_) {
            if (!sink) continue;
            try { sink->flush(); } catch (...) {}
        }
    }
};