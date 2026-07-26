#pragma once
#include "LogLevel.h"
#include <chrono>
#include <string>
#include <thread>
#include <source_location>


struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::thread::id thread_id;
    std::string message;
    std::source_location source; // TODO

    LogRecord() = default;
    LogRecord(std::chrono::system_clock::time_point ts,
              LogLevel lvl,
              std::thread::id tid,
              std::string msg,
              std::source_location src)
        : timestamp(ts), level(lvl), thread_id(tid),
          message(std::move(msg)), source(std::move(src)) {}
};
