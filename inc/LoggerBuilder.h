#pragma once
#include "Logger.h"
#include "LogSink.h"
#include <filesystem>


class LoggerBuilder {
    std::unique_ptr<Logger> logger_;

public:
    LoggerBuilder() : logger_(std::make_unique<Logger>()) {}

    LoggerBuilder& with_level(LogLevel level) {
        logger_->set_level(level);
        return *this;
    }

    LoggerBuilder& with_console_sink(bool color = true) {
        logger_->add_sink(std::make_shared<ConsoleSink>(std::cout, color));
        return *this;
    }

    LoggerBuilder& with_file_sink(const std::filesystem::path& path, bool truncate = false) {
        logger_->add_sink(std::make_shared<FileSink>(path, truncate));
        return *this;
    }

    LoggerBuilder& with_rotating_file_sink(const std::filesystem::path& path,
                                           size_t max_size,
                                           size_t max_files,
                                           bool truncate = false) {
        logger_->add_sink(std::make_shared<RotatingFileSink>(path, max_size, max_files, truncate));
        return *this;
    }

    LoggerBuilder& with_async_mode(size_t queue_size = 10'000,
                                   QueuePolicy policy = QueuePolicy::Block) {
        logger_->set_mode(Logger::Mode::Asynchronous, queue_size, policy);
        return *this;
    }

    LoggerBuilder& with_sync_mode() {
        logger_->set_mode(Logger::Mode::Synchronous);
        return *this;
    }

    LoggerBuilder& with_flush_interval(std::chrono::milliseconds interval) {
        logger_->set_flush_interval(interval);
        return *this;
    }

    std::unique_ptr<Logger> build() {
        return std::move(logger_);
    }
};
