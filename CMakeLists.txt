cmake_minimum_required(VERSION 3.16)
project(stModbus LANGUAGES C)

option(BUILD_CLI "Build internal stmodbus-cli harness" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(stmodbus STATIC
  src/modbus.c
  src/rtu/mbutils.c
)
target_include_directories(stmodbus PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(BUILD_CLI)
  add_executable(stmodbus-cli tools/stmodbus-cli/main.c)
  target_link_libraries(stmodbus-cli PRIVATE stmodbus)
  add_executable(stmodbus-master tools/stmodbus-master/main.c)
  target_link_libraries(stmodbus-master PRIVATE stmodbus)
endif()
