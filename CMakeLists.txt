cmake_minimum_required(VERSION 3.10)
project(rr_servo_host C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Clang on the MSVC ABI maps CMAKE_CXX_STANDARD 11 to -std=c++14. Pin C11/C++11.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-std=c11>)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++11>)
endif()

# LLVM Clang 18 + current MSVC STL headers.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
                          _CRT_SECURE_NO_WARNINGS)
endif()

option(RR_ENABLE_COVERAGE
       "Clang source-based coverage (llvm-cov). Host tests only." OFF)

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

if(RR_ENABLE_COVERAGE)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "RR_ENABLE_COVERAGE requires Clang (llvm-cov), not GCC/MSVC/AVR-GCC.")
  endif()
  if(WIN32)
    list(APPEND CMAKE_PROGRAM_PATH "C:/Program Files/LLVM/bin")
  endif()
  find_program(LLVM_PROFDATA NAMES llvm-profdata llvm-profdata.exe)
  find_program(LLVM_COV NAMES llvm-cov llvm-cov.exe)
  if(NOT LLVM_PROFDATA OR NOT LLVM_COV)
    message(FATAL_ERROR "RR_ENABLE_COVERAGE needs llvm-profdata and llvm-cov on PATH")
  endif()

  # Header-only lib is compiled into the test TU. Do not instrument Unity or DEBUG CLI.
  target_compile_options(rr_servo_tests PRIVATE
                         -fprofile-instr-generate -fcoverage-mapping)
  target_link_options(rr_servo_tests PRIVATE
                      -fprofile-instr-generate -fcoverage-mapping)

  set(_profraw "${CMAKE_BINARY_DIR}/rr_servo_tests.profraw")
  set(_profdata "${CMAKE_BINARY_DIR}/rr_servo_tests.profdata")
  set(_htmldir "${CMAKE_BINARY_DIR}/coverage")
  set(_libdir "${CMAKE_SOURCE_DIR}/lib/rr_servo")

  add_custom_target(coverage
    DEPENDS rr_servo_tests
    COMMAND ${CMAKE_COMMAND} -E env
            "LLVM_PROFILE_FILE=${_profraw}"
            "$<TARGET_FILE:rr_servo_tests>"
    COMMAND "${LLVM_PROFDATA}" merge -sparse "${_profraw}" -o "${_profdata}"
    COMMAND "${LLVM_COV}" report
            "$<TARGET_FILE:rr_servo_tests>"
            "-instr-profile=${_profdata}"
            "-ignore-filename-regex=third_party|Unity"
            "${_libdir}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_htmldir}"
    COMMAND "${LLVM_COV}" show
            "$<TARGET_FILE:rr_servo_tests>"
            "-instr-profile=${_profdata}"
            -format=html
            "-output-dir=${_htmldir}"
            "-ignore-filename-regex=third_party|Unity"
            "${_libdir}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Unity + llvm-cov for lib/rr_servo (HTML in ${CMAKE_BINARY_DIR}/coverage)"
    USES_TERMINAL
    VERBATIM)
endif()
