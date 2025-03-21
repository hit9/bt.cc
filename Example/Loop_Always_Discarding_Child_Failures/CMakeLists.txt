cmake_minimum_required(VERSION 3.10)
project(bt_loop_always_discarding_child_failure CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("../../Source")

# Targets
add_executable(bt_loop_always_discarding_child_failure main.cc ../../Source/bt.cc)
