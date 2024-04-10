cmake_minimum_required(VERSION 3.10)

project(bt)

set(CMAKE_CXX_STANDARD 20)

option(BT_TEST "Enables testing" OFF)

add_library(bt INTERFACE)
set_target_properties(bt PROPERTIES PUBLIC_HEADER "bt.h")

if(BT_TEST)
  add_subdirectory(tests EXCLUDE_FROM_ALL)
  add_subdirectory(example EXCLUDE_FROM_ALL)
endif()

install(
  TARGETS bt
  LIBRARY DESTINATION ${CMAK_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
