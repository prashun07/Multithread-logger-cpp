#include "../inc/LoggerInclude.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

int main() {

    auto logger = LoggerBuilder()
        .with_level(LogLevel::Debug)
        .with_console_sink(true)                       // colorized stdout
        .with_rotating_file_sink("app.log",            // 10 MB rotation, 5 backups
                                 10 * 1024 * 1024,
                                 5)
        .with_async_mode(50'000, QueuePolicy::Block)   // 50k record buffer
        .with_flush_interval(std::chrono::milliseconds(500))
        .build();

    constexpr int num_threads = 8;
    constexpr int msgs_per_thread = 10'000;

    std::vector<std::thread> threads;
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            for (int j = 0; j < msgs_per_thread; ++j) {
                INFO(*logger, "Thread {} item {} ts={}",
                           i, j,
                           std::chrono::system_clock::now().time_since_epoch().count());

                if (j % 1000 == 0)
                    DEBUG(*logger, "Checkpoint t={} j={}", i, j);
            }
        });
    }

    for (auto& t : threads) t.join();

    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    logger->flush();

    std::cout << "\n------------------------------------------------\n"
              << "Produced : " << num_threads * msgs_per_thread << " messages\n"
              << "Elapsed  : " << ms << " ms\n"
              << "Throughput: " << (num_threads * msgs_per_thread * 1000 / (ms + 1))
              << " msg/s\n"
              << "Dropped  : " << logger->dropped_messages() << '\n';
    return 0;
}