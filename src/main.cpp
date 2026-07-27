#include "../inc/LoggerInclude.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

void first_thread()
{
    LoggerDirector director;
    auto logger = director.asyncLogger(LogLevel::Debug);
    logger->log("Message from first thread");

    logger->flush();
}
void second_thread()
{
    LoggerDirector director;
    auto logger = director.syncLogger(LogLevel::Info);
    logger->log("Message from second thread");

    logger->flush();
}
void third_thread()
{
    LoggerDirector director;
    auto logger = director.asyncLogger(LogLevel::Warning);
    logger->log("Message from Third thread");

    logger->flush();
}
void fourth_thread()
{
    LoggerDirector director;
    auto logger = director.syncLogger(LogLevel::Error);
    logger->log("Message from Fourth thread");

    logger->flush();
}

int main() {
    constexpr int num_threads = 20;
    // constexpr int msgs_per_thread = 10;
    std::vector<std::thread> threads;
    auto t0 = std::chrono::high_resolution_clock::now();

    for(int i=0;i<num_threads;i++)
    {
    std::thread T1(first_thread);
    std::thread T2(second_thread);
    std::thread T3(third_thread);
    std::thread T4(fourth_thread);
    T1.join();
    T2.join();
    T3.join();
    T4.join();
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "\n------------------------------------------------\n"
              << "Produced : " << num_threads << " messages\n"
              << "Elapsed  : " << ms << " ms\n"
              << "Throughput: " << (num_threads * 1000 / (ms + 1))
              << " msg/s\n";
            //   << "Dropped  : " << logger->dropped_messages() << '\n';
   
    // constexpr int num_threads = 2;
    // constexpr int msgs_per_thread = 10;

    // std::vector<std::thread> threads;
    // auto t0 = std::chrono::high_resolution_clock::now();

    // for (int i = 0; i < num_threads; ++i) {
    //     threads.emplace_back([&, i] {
    //         for (int j = 0; j < msgs_per_thread; ++j) {
    //             logger->info("This is log Info {}, {}");
    //         }
    //     });
    // }

    // for (auto& t : threads) t.join();

    // auto t1 = std::chrono::high_resolution_clock::now();
    // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // logger->flush();

    // std::cout << "\n------------------------------------------------\n"
    //           << "Produced : " << num_threads * msgs_per_thread << " messages\n"
    //           << "Elapsed  : " << ms << " ms\n"
    //           << "Throughput: " << (num_threads * msgs_per_thread * 1000 / (ms + 1))
    //           << " msg/s\n"
    //           << "Dropped  : " << logger->dropped_messages() << '\n';
    return 0;
}