cmake_minimum_required(VERSION 3.25)
project(mtlog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Header-only library ----
add_library(mtlog INTERFACE)
target_include_directories(mtlog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:inc>
)

# ---- Demo ----
add_executable(example src/main.cpp)
target_link_libraries(example PRIVATE mtlog)
target_compile_options(example PRIVATE
    -Wall -Wextra -Wpedantic -O2 -pthread
)

# ---- Real-world benchmark ----
add_executable(benchmark src/benchmark.cpp)
target_link_libraries(benchmark PRIVATE mtlog)
target_compile_options(benchmark PRIVATE
    -Wall -Wextra -Wpedantic -O2 -pthread
)