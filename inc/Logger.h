#pragma once
#include "LogLevel.h"
#include "LogRecord.h"
#include "LogSink.h"
#include "AsyncQueue.h"
#include "LogProcessor.h"
#include <memory>
#include <vector>
#include <source_location>
#include <format>
#include <atomic>
#include <iostream>

class Logger {
public:
    enum class Mode { Synchronous, Asynchronous };

private:
    LogLevel level_ = LogLevel::Info;
    Mode mode_ = Mode::Synchronous;
    std::vector<std::shared_ptr<ILogSink>> sinks_;
    std::unique_ptr<LogProcessor> processor_;
    std::unique_ptr<AsyncLogQueue> queue_;
    std::chrono::milliseconds flush_interval_ = std::chrono::milliseconds(1000);

public:
    Logger() = default;

    ~Logger() {
        if (processor_) processor_->stop();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;

    void set_level(LogLevel level) { level_ = level; }
    LogLevel level() const { return level_; }

    void set_flush_interval(std::chrono::milliseconds interval) { flush_interval_ = interval; }

    void add_sink(std::shared_ptr<ILogSink> sink) {
        sinks_.push_back(std::move(sink));
    }

    void set_mode(Mode mode, size_t queue_size = 10'000, QueuePolicy policy = QueuePolicy::Block) {
        if (mode_ == mode) return;

        if (processor_) {
            processor_->stop();
            processor_.reset();
            queue_.reset();
        }

        mode_ = mode;
        if (mode == Mode::Asynchronous) {
            queue_ = std::make_unique<AsyncLogQueue>(queue_size, policy);
            processor_ = std::make_unique<LogProcessor>(sinks_, queue_.get(), flush_interval_);
            processor_->start();
        }
    }

    template<typename... Args>
    void log(LogLevel lvl, std::source_location loc,  const std::format_string<Args...>fmt, Args&&... args) {
    // void log(LogLevel lvl, std::source_location loc,  const char* fmt, Args&&... args) {
        if (!is_enabled(lvl, level_)) return;

        std::string msg;
        try {
            msg = std::format(fmt, std::forward<Args>(args)...);
        } catch (const std::format_error& e) {
            msg = std::string("[FORMAT ERROR: ") + e.what() + "] ";
        }

        LogRecord record{
            std::chrono::system_clock::now(),
            lvl,
            std::this_thread::get_id(),
            std::move(msg),
            loc
        };

        if (mode_ == Mode::Synchronous)
            write_sync(record);
        else
            write_async(std::move(record));
    }

    void flush() {
        if (mode_ == Mode::Synchronous) {
            for (auto& sink : sinks_) if (sink) sink->flush();
        } else if (queue_) {
            while (queue_->size() > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            for (auto& sink : sinks_) if (sink) sink->flush();
        }
    }

    size_t dropped_messages() const {
        return queue_ ? queue_->dropped() : 0;
    }

private:
    void write_sync(const LogRecord& record) {
        for (auto& sink : sinks_) {
            if (!sink) continue;
            try {
                sink->write(record);
            } catch (const std::exception& e) {
                std::cerr << "[LOG ERROR] " << e.what() << '\n';
            }
        }
    }

    void write_async(LogRecord record) {
        if (!queue_ || !queue_->push(std::move(record)))
            write_sync(record);   // fallback if queue is full / unavailable
    }
};


/* ---------- Macros for ergonomic source location ---------- */
#ifndef DISABLE_TRACE
#  define TRACE(logger, ...) (logger).log(LogLevel::Trace, std::source_location::current(), __VA_ARGS__)
#else
#  define TRACE(logger, ...) ((void)0)
#endif

#ifndef DISABLE_DEBUG
#  define DEBUG(logger, ...) (logger).log(LogLevel::Debug, std::source_location::current(), __VA_ARGS__)
#else
#  define DEBUG(logger, ...) ((void)0)
#endif

#define INFO(logger, ...)  (logger).log(LogLevel::Info,    std::source_location::current(), __VA_ARGS__)
#define WARN(logger, ...)  (logger).log(LogLevel::Warning, std::source_location::current(), __VA_ARGS__)
#define ERROR(logger, ...) (logger).log(LogLevel::Error,   std::source_location::current(), __VA_ARGS__)
#define FATAL(logger, ...) (logger).log(LogLevel::Fatal,   std::source_location::current(), __VA_ARGS__)