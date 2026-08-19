cmake_minimum_required(VERSION 3.10)
project(rr_servo_host C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/Unity/src/unity.c")
  message(FATAL_ERROR "Unity submodule missing. Run: git submodule update --init --recursive")
endif()

add_library(unity_host STATIC third_party/Unity/src/unity.c)
target_include_directories(unity_host PUBLIC third_party/Unity/src)

add_executable(rr_servo_tests tests/test_rr_servo.cpp)
target_include_directories(rr_servo_tests PRIVATE lib/rr_servo/src)
target_link_libraries(rr_servo_tests PRIVATE unity_host)

add_executable(rr_servo_debug_cli host/debug_cli.cpp)
target_include_directories(rr_servo_debug_cli PRIVATE lib/rr_servo/src)
target_compile_definitions(rr_servo_debug_cli PRIVATE DEBUG)
