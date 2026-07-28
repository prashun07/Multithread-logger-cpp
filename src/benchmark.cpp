#include "../inc/LoggerInclude.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Ns = std::chrono::nanoseconds;

struct LatencyStats {
    int64_t samples = 0;
    int64_t p50_ns = 0;
    int64_t p95_ns = 0;
    int64_t p99_ns = 0;
    int64_t max_ns = 0;
};

struct BenchmarkResult {
    std::string name;
    int threads = 0;
    int msgs_per_thread = 0;
    int64_t produce_ms = 0;
    int64_t drain_ms = 0;
    size_t total_messages = 0;
    size_t dropped = 0;
    double throughput_msg_s = 0.0;
    LatencyStats latency{};
};

struct Workload {
    int threads = 16;
    int msgs_per_thread = 10'000;
    LogLevel level = LogLevel::Info;
    bool include_debug_noise = true;
    bool burst_pattern = false;
};

struct LoggerSetup {
    bool async_mode = true;
    bool console_sink = false;
    QueuePolicy policy = QueuePolicy::Block;
    size_t queue_size = 50'000;
    std::chrono::milliseconds flush_interval{500};
    size_t rotate_bytes = 10 * 1024 * 1024;
    size_t rotate_backups = 5;
};

LatencyStats compute_latency_stats(std::vector<int64_t>& samples) {
    LatencyStats stats{};
    if (samples.empty()) return stats;

    std::sort(samples.begin(), samples.end());
    stats.samples = static_cast<int64_t>(samples.size());
    stats.p50_ns = samples[static_cast<size_t>(stats.samples * 50 / 100)];
    stats.p95_ns = samples[static_cast<size_t>(stats.samples * 95 / 100)];
    stats.p99_ns = samples[static_cast<size_t>(stats.samples * 99 / 100)];
    stats.max_ns = samples.back();
    return stats;
}

std::unique_ptr<Logger> make_logger(const std::filesystem::path& log_dir,
                                    const LoggerSetup& setup,
                                    LogLevel level) {
    const auto log_path = log_dir / "bench.log";

    auto logger = std::make_unique<Logger>();
    logger->set_level(level);
    logger->set_flush_interval(setup.flush_interval);

    if (setup.console_sink) {
        static std::ofstream null_out("/dev/null");
        logger->add_sink(std::make_shared<ConsoleSink>(null_out, false));
    }

    logger->add_sink(std::make_shared<RotatingFileSink>(
        log_path, setup.rotate_bytes, setup.rotate_backups, true));

    if (setup.async_mode) {
        logger->set_mode(Logger::Mode::Asynchronous, setup.queue_size, setup.policy);
    } else {
        logger->set_mode(Logger::Mode::Synchronous);
    }

    return logger;
}

void worker_loop(Logger& logger,
                 int thread_index,
                 int msgs_per_thread,
                 LogLevel level,
                 bool include_debug_noise,
                 bool burst_pattern,
                 std::vector<int64_t>& local_latencies) {
    std::mt19937 rng(static_cast<uint32_t>(thread_index + 1));
    std::uniform_int_distribution<int> request_id_dist(1'000'000, 9'999'999);
    std::uniform_int_distribution<int> status_dist(200, 599);
    std::uniform_int_distribution<int> latency_dist(1, 250);

    local_latencies.reserve(static_cast<size_t>(msgs_per_thread));

    for (int i = 0; i < msgs_per_thread; ++i) {
        if (burst_pattern && i > 0 && (i % 500) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        const auto t0 = Clock::now();

        if (include_debug_noise && (i % 17) == 0) {
            logger.log_dump(
                LogLevel::Debug,
                std::source_location::current(),
                "trace t={} req={} stage=validate attempt={}",
                thread_index,
                request_id_dist(rng),
                i);
        } else if ((i % 113) == 0) {
            logger.log_dump(
                LogLevel::Warning,
                std::source_location::current(),
                "slow_request t={} req={} status={} upstream_ms={}",
                thread_index,
                request_id_dist(rng),
                status_dist(rng),
                latency_dist(rng));
        } else {
            logger.log_dump(
                level,
                std::source_location::current(),
                "handled_request t={} req={} status={} duration_ms={}",
                thread_index,
                request_id_dist(rng),
                status_dist(rng),
                latency_dist(rng));
        }

        const auto t1 = Clock::now();
        local_latencies.push_back(
            std::chrono::duration_cast<Ns>(t1 - t0).count());
    }
}

BenchmarkResult run_scenario(const std::string& name,
                             const Workload& workload,
                             const LoggerSetup& setup,
                             const std::filesystem::path& log_dir) {
    BenchmarkResult result{};
    result.name = name;
    result.threads = workload.threads;
    result.msgs_per_thread = workload.msgs_per_thread;
    result.total_messages =
        static_cast<size_t>(workload.threads) * static_cast<size_t>(workload.msgs_per_thread);

    std::filesystem::create_directories(log_dir);
    auto logger = make_logger(log_dir, setup, workload.level);

    std::vector<std::thread> threads;
    std::vector<std::vector<int64_t>> per_thread_latencies(static_cast<size_t>(workload.threads));

    const auto produce_start = Clock::now();

    for (int t = 0; t < workload.threads; ++t) {
        threads.emplace_back([&, t] {
            worker_loop(*logger,
                        t,
                        workload.msgs_per_thread,
                        workload.level,
                        workload.include_debug_noise,
                        workload.burst_pattern,
                        per_thread_latencies[static_cast<size_t>(t)]);
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    const auto produce_end = Clock::now();
    result.produce_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(produce_end - produce_start).count();

    const auto drain_start = Clock::now();
    logger->flush();
    const auto drain_end = Clock::now();
    result.drain_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(drain_end - drain_start).count();

    result.dropped = logger->dropped_messages();

    const int64_t total_ms = result.produce_ms + result.drain_ms;
    result.throughput_msg_s =
        total_ms > 0
            ? static_cast<double>(result.total_messages) * 1000.0 / static_cast<double>(total_ms)
            : static_cast<double>(result.total_messages);

    std::vector<int64_t> all_latencies;
    all_latencies.reserve(result.total_messages);
    for (auto& bucket : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), bucket.begin(), bucket.end());
    }
    result.latency = compute_latency_stats(all_latencies);

    return result;
}

void print_latency(const LatencyStats& stats) {
    std::cout << "  Latency (producer, ns): "
              << "samples=" << stats.samples
              << " p50=" << stats.p50_ns
              << " p95=" << stats.p95_ns
              << " p99=" << stats.p99_ns
              << " max=" << stats.max_ns
              << '\n';
}

void print_result(const BenchmarkResult& r) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "\n[" << r.name << "]\n"
              << "  Threads         : " << r.threads << '\n'
              << "  Msgs/thread     : " << r.msgs_per_thread << '\n'
              << "  Total messages  : " << r.total_messages << '\n'
              << "  Produce time    : " << r.produce_ms << " ms\n"
              << "  Drain time      : " << r.drain_ms << " ms\n"
              << "  Throughput      : " << r.throughput_msg_s << " msg/s\n"
              << "  Dropped         : " << r.dropped << '\n';
    print_latency(r.latency);
}

int parse_int_arg(int argc, char** argv, const char* flag, int default_value) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == flag) {
            return std::stoi(argv[i + 1]);
        }
    }
    return default_value;
}

std::filesystem::path make_run_directory() {
    const auto root = std::filesystem::current_path() / "benchmark-runs";
    std::filesystem::create_directories(root);

    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto run_dir = root / ("run-" + std::to_string(stamp));
    std::filesystem::create_directories(run_dir);
    return run_dir;
}

void print_header(const Workload& workload, const std::filesystem::path& run_dir) {
    std::cout << "============================================================\n"
              << " Multithreaded Logger — Real-World Benchmark\n"
              << "============================================================\n"
              << "Run directory : " << run_dir << '\n'
              << "Threads       : " << workload.threads << '\n'
              << "Msgs/thread   : " << workload.msgs_per_thread << '\n'
              << "Total msgs    : "
              << (static_cast<size_t>(workload.threads) * static_cast<size_t>(workload.msgs_per_thread))
              << "\n\n"
              << "Scenarios simulate a shared logger under concurrent web-server style load:\n"
              << "  • mixed INFO/WARN/DEBUG messages with structured fields\n"
              << "  • rotating file sink (production default)\n"
              << "  • optional console sink (development setup)\n"
              << "  • async vs sync, burst traffic, and queue backpressure\n";
}

} // namespace

int main(int argc, char** argv) {
    Workload workload{};
    workload.threads = parse_int_arg(argc, argv, "--threads", workload.threads);
    workload.msgs_per_thread = parse_int_arg(argc, argv, "--msgs", workload.msgs_per_thread);

    const auto run_dir = make_run_directory();
    print_header(workload, run_dir);

    std::vector<BenchmarkResult> results;

    // 1) Production async: file-only, shared logger, steady load
    {
        LoggerSetup setup{};
        setup.async_mode = true;
        setup.console_sink = false;
        setup.policy = QueuePolicy::Block;
        results.push_back(run_scenario("async_file_production", workload, setup, run_dir / "async_file_production"));
    }

    // 2) Production sync baseline for comparison
    {
        LoggerSetup setup{};
        setup.async_mode = false;
        setup.console_sink = false;
        results.push_back(run_scenario("sync_file_production", workload, setup, run_dir / "sync_file_production"));
    }

    // 3) Development setup: async + console + file (higher sink contention)
    {
        LoggerSetup setup{};
        setup.async_mode = true;
        setup.console_sink = true;
        setup.policy = QueuePolicy::Block;
        results.push_back(run_scenario("async_file_console_dev", workload, setup, run_dir / "async_file_console_dev"));
    }

    // 4) Burst traffic: threads idle briefly every 500 logs (batch request handling)
    {
        Workload burst = workload;
        burst.burst_pattern = true;

        LoggerSetup setup{};
        setup.async_mode = true;
        setup.console_sink = false;
        results.push_back(run_scenario("async_burst_traffic", burst, setup, run_dir / "async_burst_traffic"));
    }

    // 5) Queue pressure: small queue + Drop policy under sustained load
    {
        LoggerSetup setup{};
        setup.async_mode = true;
        setup.console_sink = false;
        setup.queue_size = 2'048;
        setup.policy = QueuePolicy::Drop;
        results.push_back(run_scenario("async_queue_drop_pressure", workload, setup, run_dir / "async_queue_drop_pressure"));
    }

    // 6) Queue pressure with Block policy (measures backpressure on producers)
    {
        LoggerSetup setup{};
        setup.async_mode = true;
        setup.console_sink = false;
        setup.queue_size = 2'048;
        setup.policy = QueuePolicy::Block;
        results.push_back(run_scenario("async_queue_block_pressure", workload, setup, run_dir / "async_queue_block_pressure"));
    }

    std::cout << "\n============================================================\n"
              << " Results\n"
              << "============================================================\n";

    for (const auto& r : results) {
        print_result(r);
    }

    if (results.size() >= 2) {
        const auto& async_result = results[0];
        const auto& sync_result = results[1];
        const double speedup =
            sync_result.throughput_msg_s > 0.0
                ? async_result.throughput_msg_s / sync_result.throughput_msg_s
                : 0.0;

        std::cout << "\nSummary\n"
                  << "  Async vs sync throughput speedup: "
                  << std::setprecision(2) << speedup << "x\n"
                  << "  Async producer p99 latency       : "
                  << async_result.latency.p99_ns << " ns\n"
                  << "  Sync producer p99 latency        : "
                  << sync_result.latency.p99_ns << " ns\n";
    }

    std::cout << "\nLogs written under: " << run_dir << '\n';
    return 0;
}
