cmake_minimum_required(VERSION 3.10)
project(bt_action_empty_example CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("../../Source")

# Targets
add_executable(bt_action_empty_example main.cc ../../Source/bt.cc)
