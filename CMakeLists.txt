cmake_minimum_required(VERSION 3.25)
project(mtlog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Header-only library ----
add_library(inc INTERFACE)
target_include_directories(inc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ---- Test ----
add_executable(test src/main.cpp)
target_link_libraries(test PRIVATE inc)
target_compile_options(test PRIVATE
    -Wall -Wextra -Wpedantic -O2 -pthread
)