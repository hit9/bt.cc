cmake_minimum_required(VERSION 3.10)
project(bt_example_onsignal CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("../..")

# Targets
add_executable(bt_example_onsignal main.cc)
