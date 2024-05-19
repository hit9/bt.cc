cmake_minimum_required(VERSION 3.10)

project(bt)

set(CMAKE_CXX_STANDARD 20)

add_library(bt SHARED bt.cc)
set_target_properties(bt PROPERTIES PUBLIC_HEADER "bt.h")

install(
  TARGETS bt
  LIBRARY DESTINATION ${CMAK_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
