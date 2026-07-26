# Multithread-logger-cpp
# mtlog – Production-Grade Multithreaded C++ Logger

A high-performance, header-only C++20 logging library designed for clean architecture, real-time throughput, and flexible sink configuration.

---

## Features

* **Clean Architecture** – Strategy pattern for sinks (`Console`, `File`, `RotatingFile`) and formatters (`Text`, `JSON`).
* **Dual Threading Models**
  * **Synchronous** – lowest latency for single-threaded or low-contention scenarios.
  * **Asynchronous** – bounded MPMC queue with a dedicated backend thread; producers never block on disk I/O.
* **Backpressure Policies** – `Block`, `Drop`, or `Overwrite` when the async queue is full.
* **Log Rotation** – size-based rotation with configurable backup count.
* **Compile-Time Filtering** – `MTLOG_TRACE` / `MTLOG_DEBUG` can be `#define`d away to zero cost in Release builds.
* **Source Location Capture** – automatic file, line, and function injection via macros (zero manual boilerplate).
* **Type-Safe Formatting** – built on C++20 `std::format` (or drop-in replaceable with `{fmt}`).
* **Zero External Dependencies** – only standard library.

---

## Project Structure
multithread-logger-cpp/
├── CMakeLists.txt
├── include/
│   └── inc/
│       ├── LogLevel.h
│       ├── LogRecord.h
│       ├── LogFormatter.h
│       ├── LogSink.h
│       ├── AsyncQueue.h
│       ├── LogProcessor.h
│       ├── Logger.h
│       ├── LoggerBuilder.h
│       └── LoggerInclude.h        # umbrella header
└── src/
    └── main.cpp


---

## Quick Start

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./example

```cpp
#include "../inc/LoggerInclude.h"

int main() {
    auto logger = LoggerBuilder()
        .with_level(LogLevel::Info)
        .with_console_sink(true)
        .with_file_sink("output.log")
        .build();

    INFO(*logger, "Hello from {}", "multithread-logger-cpp");
    return 0;
}
```
Architecture Deep Dive
1. Frontend (Logger)
Receives variadic, type-safe log calls.
Formats the message before entering the critical section (or queue) so that CPU-heavy formatting runs in parallel across cores.
In Sync mode, iterates sinks and writes directly.
In Async mode, moves the LogRecord into a pre-allocated ring buffer.
2. Queue (AsyncLogQueue)
Pre-allocated circular buffer of std::optional<LogRecord>—no heap allocations on the hot path.
Block – producer waits (condition variable) for space.
Drop – message discarded; dropped() exposes the counter.
Overwrite – oldest message evicted (useful for telemetry where recency matters).
3. Backend (LogProcessor)
Dedicated thread drains the queue in batches.
Dispatches to every sink.
Periodic flush (configurable interval) amortizes fsync cost.
4. Sinks (LogSink)
| Sink               | Description                                                    |
| ------------------ | -------------------------------------------------------------- |
| `ConsoleSink`      | Thread-safe `std::cout`/`std::cerr` with optional ANSI colors. |
| `FileSink`         | Buffered file append.                                          |
| `RotatingFileSink` | Extends `FileSink`; rotates when `max_size` is exceeded.       |

Each sink owns its ILogFormatter, allowing e.g., JSON to file and pretty text to console simultaneously.

# Thread Safety & Real-Time Guarantees
| Aspect            | Guarantee                                                                                                                                                                                                                                          |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Frontend API**  | Thread-safe. Multiple threads may log concurrently.                                                                                                                                                                                                |
| **Sync mode**     | Mutex per sink; formatting occurs outside the lock to minimize contention.                                                                                                                                                                         |
| **Async mode**    | Producer fast path = lock + `memcpy` into pre-allocated slot + signal. No I/O on caller thread.                                                                                                                                                    |
| **Real-time tip** | For hard real-time (sub-µs latency), switch the `AsyncLogQueue` to a **lock-free SPSC ring buffer** per thread and batch-flush to the backend. The current mutex-based queue is suitable for soft real-time and general high-throughput workloads. |

# Performance Tips
1. Prefer Async mode when >2 threads are logging heavily.
2. Increase queue size if dropped_messages() is non-zero.
3. Use QueuePolicy::Drop for latency-sensitive paths where occasional log loss is acceptable.
4. Compile out levels you don't need in Release (MTLOG_DISABLE_TRACE).
5. Share formatters across sinks if all outputs use the same format (avoids duplicate formatting CPU cost).

# Compiler Support
GCC ≥ 13
Clang ≥ 17
MSVC ≥ 2019 (16.11+)

# License
MIT License. See LICENSE file for details.
