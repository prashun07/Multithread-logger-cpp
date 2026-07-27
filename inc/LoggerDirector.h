#include "LoggerBuilder.h"

class LoggerDirector{
    public:
    std::unique_ptr<Logger>  asyncLogger(LogLevel level)
    {
        return LoggerBuilder().with_level(level)
        .with_console_sink(true)                       // colorized stdout
        .with_rotating_file_sink("app.log",            // 10 MB rotation, 5 backups
                                 10 * 1024 * 1024,
                                 5)
        .with_async_mode(50'000, QueuePolicy::Block)   // 50k record buffer
        .with_flush_interval(std::chrono::milliseconds(500))
        .build();
    }

   std::unique_ptr<Logger> syncLogger(LogLevel level)
    {
        return LoggerBuilder().with_level(level)
        .with_console_sink(true)                       // colorized stdout
        .with_rotating_file_sink("app.log",            // 10 MB rotation, 5 backups
                                 10 * 1024 * 1024,
                                 5)
        .with_async_mode(50'000, QueuePolicy::Block)   // 50k record buffer
        .with_flush_interval(std::chrono::milliseconds(500))
        .build();
    }

};