cmake_minimum_required(VERSION 3.16)
project(orip VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ORIP_BUILD_TESTS "Build unit tests" ON)
option(ORIP_BUILD_EXAMPLES "Build examples" ON)
option(ORIP_BUILD_SHARED "Build shared library" OFF)

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
    )
    target_link_libraries(orip_tests PRIVATE orip GTest::gtest_main)

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
