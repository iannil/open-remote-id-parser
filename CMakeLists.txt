cmake_minimum_required(VERSION 3.16)
project(orip VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ORIP_BUILD_TESTS "Build unit tests" ON)
option(ORIP_BUILD_EXAMPLES "Build examples" ON)
option(ORIP_BUILD_SHARED "Build shared library" OFF)
option(ORIP_ENABLE_COVERAGE "Enable code coverage" OFF)
option(ORIP_ENABLE_SANITIZERS "Enable sanitizers (ASan, UBSan)" OFF)
option(ORIP_ENABLE_TSAN "Enable ThreadSanitizer for data race detection" OFF)
option(ORIP_BUILD_FUZZ "Build fuzz targets (requires Clang)" OFF)
option(ORIP_BUILD_BENCHMARKS "Build performance benchmarks" OFF)

# Core library sources
set(ORIP_SOURCES
    src/core/parser.cpp
    src/core/session_manager.cpp
    src/core/orip_c.cpp
    src/protocols/astm_f3411.cpp
    src/protocols/wifi_decoder.cpp
    src/protocols/asd_stan.cpp
    src/protocols/cn_rid.cpp
    src/analysis/anomaly_detector.cpp
    src/analysis/trajectory_analyzer.cpp
    src/utils/bit_reader.cpp
)

# Create static library
add_library(orip STATIC ${ORIP_SOURCES})
target_include_directories(orip PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Create shared library (optional)
if(ORIP_BUILD_SHARED)
    add_library(orip_shared SHARED ${ORIP_SOURCES})
    target_include_directories(orip_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(orip_shared PROPERTIES
        OUTPUT_NAME orip
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    # Export symbols for shared library
    if(WIN32)
        target_compile_definitions(orip_shared PRIVATE ORIP_EXPORTS)
    else()
        target_compile_options(orip_shared PRIVATE -fvisibility=default)
    endif()
endif()

# Compiler warnings
target_compile_options(orip PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Code coverage
if(ORIP_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling code coverage")
    target_compile_options(orip PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(orip PRIVATE --coverage)
endif()

# Sanitizers
if(ORIP_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling sanitizers (ASan, UBSan)")
    set(SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(orip PRIVATE ${SANITIZER_FLAGS})
    target_link_options(orip PRIVATE ${SANITIZER_FLAGS})
endif()

# ThreadSanitizer (mutually exclusive with ASan)
if(ORIP_ENABLE_TSAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling ThreadSanitizer")
    set(TSAN_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
    target_compile_options(orip PRIVATE ${TSAN_FLAGS})
    target_link_options(orip PRIVATE ${TSAN_FLAGS})
endif()

# Tests
if(ORIP_BUILD_TESTS)
    enable_testing()

    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(orip_tests
        tests/test_main.cpp
        tests/test_astm_f3411.cpp
        tests/test_bit_reader.cpp
        tests/test_c_api.cpp
        tests/test_wifi_decoder.cpp
        tests/test_asd_stan.cpp
        tests/test_analysis.cpp
        tests/test_boundary.cpp
        tests/test_thread_safety.cpp
        tests/test_protocol_detection.cpp
        tests/test_malicious_input.cpp
    )
    target_link_libraries(orip_tests PRIVATE orip GTest::gtest_main)

    # Coverage for tests
    if(ORIP_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(orip_tests PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(orip_tests PRIVATE --coverage)
    endif()

    # Sanitizers for tests
    if(ORIP_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(orip_tests PRIVATE ${SANITIZER_FLAGS})
        target_link_options(orip_tests PRIVATE ${SANITIZER_FLAGS})
    endif()

    # ThreadSanitizer for tests
    if(ORIP_ENABLE_TSAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(orip_tests PRIVATE ${TSAN_FLAGS})
        target_link_options(orip_tests PRIVATE ${TSAN_FLAGS})
    endif()

    include(GoogleTest)
    gtest_discover_tests(orip_tests)
endif()

# Examples
if(ORIP_BUILD_EXAMPLES)
    add_executable(parse_demo examples/parse_demo.cpp)
    target_link_libraries(parse_demo PRIVATE orip)

    add_executable(parse_demo_c examples/parse_demo_c.c)
    target_link_libraries(parse_demo_c PRIVATE orip)
endif()

# =============================================================================
# Fuzz Testing (libFuzzer)
# =============================================================================
if(ORIP_BUILD_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(WARNING "Fuzzing requires Clang compiler. Current: ${CMAKE_CXX_COMPILER_ID}")
    else()
        message(STATUS "Building fuzz targets with libFuzzer")

        set(FUZZ_FLAGS -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)

        add_executable(fuzz_parser fuzz/fuzz_parser.cpp)
        target_link_libraries(fuzz_parser PRIVATE orip)
        target_compile_options(fuzz_parser PRIVATE ${FUZZ_FLAGS})
        target_link_options(fuzz_parser PRIVATE ${FUZZ_FLAGS})

        # Create corpus directory
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz_corpus)

        # Add custom target to run fuzzing
        add_custom_target(run_fuzz
            COMMAND fuzz_parser ${CMAKE_BINARY_DIR}/fuzz_corpus -max_len=512 -timeout=5 -max_total_time=60
            DEPENDS fuzz_parser
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running fuzz tests for 60 seconds..."
        )
    endif()
endif()

# =============================================================================
# Performance Benchmarks (Google Benchmark)
# =============================================================================
if(ORIP_BUILD_BENCHMARKS)
    message(STATUS "Building performance benchmarks")

    # Fetch Google Benchmark
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    add_executable(orip_benchmarks
        benchmarks/bench_parser.cpp
    )
    target_link_libraries(orip_benchmarks PRIVATE orip benchmark::benchmark)

    # Add custom target to run benchmarks
    add_custom_target(run_benchmarks
        COMMAND orip_benchmarks --benchmark_format=console --benchmark_counters_tabular=true
        DEPENDS orip_benchmarks
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running performance benchmarks..."
    )
endif()

# Install
install(TARGETS orip
    EXPORT oripTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)

# Package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/oripConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
