cmake_minimum_required(VERSION 3.10)

project(bt_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

enable_testing()

include_directories(".." ".")

find_package(Catch2 CONFIG REQUIRED)

# Targets
file(GLOB TEST_SOURCES *_test.cc)
file(GLOB BENCHMARK_SOURCES *_benchmark.cc)
add_executable(bt_tests ${TEST_SOURCES})
add_executable(bt_benchmark ${BENCHMARK_SOURCES})

target_link_libraries(bt_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(bt_benchmark PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(bt_tests)
