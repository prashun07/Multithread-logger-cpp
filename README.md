# 🪵 Production-Grade Multithreaded C++ Logger

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?style=for-the-badge&logo=c%2B%2B" />
  <img src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=for-the-badge&logo=cmake" />
  <img src="https://img.shields.io/badge/GCC-%E2%89%A513-green?style=for-the-badge&logo=gnu" />
  <img src="https://img.shields.io/badge/Clang-%E2%89%A517-blue?style=for-the-badge&logo=llvm" />
  <img src="https://img.shields.io/badge/MSVC-2019%2B-5C2D91?style=for-the-badge&logo=visual-studio" />
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge" />
</p>

<p align="center">
  <b>Header-only</b> · <b>Zero Dependencies</b> · <b>Type-Safe</b> · <b>High Throughput</b>
</p>

---

## 📑 Table of Contents

- [Features](#-features)
- [Quick Start](#-quick-start)
- [Installation](#-installation)
- [Project Structure](#-project-structure)
- [API Reference](#-api-reference)
- [Architecture](#-architecture)
- [Performance](#-performance)
- [Examples](#-examples)
- [Compiler Support](#-compiler-support)
- [License](#-license)

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 🧩 **Clean Architecture** | Strategy pattern for sinks (`Console`, `File`, `RotatingFile`) and formatters (`Text`, `JSON`). |
| ⚡ **Dual Threading Models** | **Synchronous** — lowest latency for single-threaded paths. **Asynchronous** — bounded MPMC queue with a dedicated backend thread; producers never block on disk I/O. |
| 🛡️ **Backpressure Policies** | `Block`, `Drop`, or `Overwrite` when the async queue is full. You control the trade-off. |
| 🔄 **Log Rotation** | Size-based rotation with configurable backup count. |
| 🔕 **Compile-Time Filtering** | `DISABLE_TRACE` / `DISABLE_DEBUG` can be `#define`d away to **zero cost** in Release builds. |
| 📍 **Source Location Capture** | Automatic file, line, and function injection via macros — zero manual boilerplate. |
| 🎨 **Type-Safe Formatting** | Built on C++20 `std::format` (or drop-in replaceable with `{fmt}`). |
| 📦 **Zero External Dependencies** | Only the standard library. |

---

## 🚀 Quick Start

### 1. Clone & Build

```bash
git clone https://github.com/prashun07/Multithread-logger-cpp.git
cd Multithread-logger-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./mtlog        # Linux/macOS
# mtlog.exe    # Windows
```

### 2. Minimal Example

```cpp
#include "LoggerInclude.h"

int main() {
    auto logger = LoggerBuilder()
        .with_level(LogLevel::Info)
        .with_console_sink(true)          // colorized stdout
        .with_file_sink("app.log")        // plain text file
        .build();

    INFO(*logger, "Hello from {}", "mtlog");
    return 0;
}
```

---

## 📦 Installation

Since **logger** is header-only, you can simply copy the `inc/` directory into your project:

```bash
cp -r inc/ /path/to/your/project/third_party/logger/
```

Then add it to your `CMakeLists.txt`:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(your_app src/main.cpp)
target_include_directories(your_app PRIVATE third_party/mtlog)
```

Or use it as a subdirectory:

```cmake
add_subdirectory(Multithread-logger-cpp)
target_link_libraries(your_app PRIVATE mtlog)
```

---

## 📁 Project Structure

```
Multithread-logger-cpp/
├── CMakeLists.txt          # C++20, header-only build
├── inc/                    # All headers (header-only library)
│   ├── LogLevel.h            # LogLevel enum + helpers
│   ├── LogRecord.h           # LogRecord struct
│   ├── LogFormatter.h        # TextFormatter & JsonFormatter
│   ├── LogSink.h             # ConsoleSink, FileSink, RotatingFileSink
│   ├── AsyncQueue.h          # Bounded MPMC queue (Block/Drop/Overwrite)
│   ├── LogProcessor.h        # Dedicated backend drain thread
│   ├── Logger.h              # Core Logger + macros
│   ├── LoggerBuilder.h       # Fluent configuration API
│   └── LoggerInclude.h       # ☂️ Umbrella header — include this
└── src/
    └── main.cpp              # Benchmark / demo
```

---

## 📖 API Reference

### LoggerBuilder

Fluent API to configure your logger. All methods return `LoggerBuilder&` for chaining.

| Method | Signature | Description |
|--------|-----------|-------------|
| `with_level` | `(LogLevel level)` | Minimum log level to emit |
| `with_console_sink` | `(bool color = true)` | Thread-safe `stdout` with optional ANSI colors |
| `with_file_sink` | `(const std::filesystem::path& path, bool truncate = false)` | Buffered file append |
| `with_rotating_file_sink` | `(path, size_t max_size, size_t max_files, bool truncate = false)` | Auto-rotate when `max_size` exceeded |
| `with_async_mode` | `(size_t queue_size = 10'000, QueuePolicy policy = QueuePolicy::Block)` | Enable async backend |
| `with_sync_mode` | `()` | Synchronous direct-write mode |
| `with_flush_interval` | `(std::chrono::milliseconds interval)` | Backend flush interval (default 1000ms) |
| `build` | `() -> std::unique_ptr<Logger>` | Finalize and return the logger |

### Logging Macros

All macros inject `std::source_location::current()` automatically.

```cpp
TRACE(logger,  "trace message: {}",  value);   // stripped with -DDISABLE_TRACE
DEBUG(logger,  "debug message: {}",  value);   // stripped with -DDISABLE_DEBUG
INFO (logger,  "info message: {}",   value);
WARN (logger,  "warn message: {}",   value);
ERROR(logger,  "error message: {}",  value);
FATAL(logger,  "fatal message: {}",  value);
```

> 💡 **Tip:** Define `DISABLE_TRACE` and/or `DISABLE_DEBUG` before including `LoggerInclude.h` to completely compile away trace/debug logs in Release builds.

### Logger Methods

| Method | Description |
|--------|-------------|
| `log(lvl, loc, fmt, args...)` | Low-level templated log call (macros wrap this) |
| `flush()` | Block until all queued messages are written |
| `dropped_messages()` | Number of messages dropped (async mode only) |
| `set_level(LogLevel)` | Runtime level change |
| `add_sink(std::shared_ptr<ILogSink>)` | Attach a custom sink |

---

## 🏗️ Architecture

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐     ┌─────────────┐
│   Frontend  │────▶│    Queue     │────▶│   Backend   │────▶│    Sinks    │
│  (Logger)   │     │(AsyncLogQueue)│     │(LogProcessor)│     │             │
│  + Macros   │     │ Block/Drop/  │     │  Drain +    │     │ • Console   │
│             │     │  Overwrite   │     │   Batch     │     │ • File      │
└─────────────┘     └──────────────┘     └─────────────┘     │ • Rotating  │
                                                               └─────────────┘
```

1. **Frontend (`Logger`)** — Receives variadic, type-safe log calls. Formats the message *before* entering the critical section so CPU-heavy formatting runs in parallel across cores.
2. **Queue (`AsyncLogQueue`)** — Pre-allocated circular buffer of `std::optional<LogRecord>`. No heap allocations on the hot path.
3. **Backend (`LogProcessor`)** — Dedicated thread drains the queue in batches and dispatches to every sink. Periodic flush amortizes `fsync` cost.
4. **Sinks (`ILogSink`)** — Each sink owns its `ILogFormatter`, allowing e.g. JSON to file and pretty text to console simultaneously.

---

## ⚡ Performance & Thread Safety

| Aspect | Guarantee |
|--------|-----------|
| **Frontend API** | Thread-safe. Multiple threads may log concurrently. |
| **Sync mode** | Mutex per sink; formatting occurs **outside** the lock to minimize contention. |
| **Async mode** | Producer fast path = lock + `memcpy` into pre-allocated slot + signal. No I/O on caller thread. |
| **Real-time tip** | For hard real-time (sub-µs latency), swap the `AsyncLogQueue` for a **lock-free SPSC ring buffer** per thread and batch-flush to the backend. The current mutex-based queue is suitable for soft real-time and general high-throughput workloads. |

### Performance Tips

1. Prefer **Async mode** when >2 threads are logging heavily.
2. Increase queue size if `dropped_messages()` is non-zero.
3. Use `QueuePolicy::Drop` for latency-sensitive paths where occasional log loss is acceptable.
4. Compile out levels you don't need in Release (`-DDISABLE_TRACE -DDISABLE_DEBUG`).
5. Share formatters across sinks if all outputs use the same format (avoids duplicate formatting CPU cost).

---

## 💡 Examples

### Async + Rotating File + Multithreaded Benchmark

```cpp
#include "LoggerInclude.h"
#include <thread>
#include <vector>

int main() {
    auto logger = LoggerBuilder()
        .with_level(LogLevel::Debug)
        .with_console_sink(true)
        .with_rotating_file_sink("app.log", 10 * 1024 * 1024, 5)  // 10 MB, 5 backups
        .with_async_mode(50'000, QueuePolicy::Block)
        .with_flush_interval(std::chrono::milliseconds(500))
        .build();

    constexpr int num_threads = 8;
    constexpr int msgs_per_thread = 10'000;

    std::vector<std::thread> threads;
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            for (int j = 0; j < msgs_per_thread; ++j) {
                INFO(*logger, "Thread {} item {} ts={}", i, j,
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
              << "Throughput: " << (num_threads * msgs_per_thread * 1000 / (ms + 1)) << " msg/s\n"
              << "Dropped  : " << logger->dropped_messages() << '\n';
    return 0;
}
```

### Custom Formatter Pattern

```cpp
auto sink = std::make_shared<ConsoleSink>(std::cout, true);
sink->set_formatter(std::make_unique<TextFormatter>(
    "[{level}] {message} — {file}:{line}"
));
logger->add_sink(sink);
```

### JSON Output to File

```cpp
auto file_sink = std::make_shared<FileSink>("events.json");
file_sink->set_formatter(std::make_unique<JsonFormatter>());
logger->add_sink(file_sink);
```

---

## 🖥️ Compiler Support

| Compiler | Minimum Version |
|----------|-----------------|
| GCC | ≥ 13 |
| Clang | ≥ 17 |
| MSVC | ≥ 2019 (16.11+) |

---

## 📄 License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for details.

---

<p align="center">
  Built with ❤️ by <a href="https://github.com/prashun07">@prashun07</a>
</p>
