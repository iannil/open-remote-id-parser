cmake_minimum_required(VERSION 3.22.1)
project(orip_jni)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to core library
set(ORIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

# Core library sources
set(ORIP_SOURCES
    ${ORIP_ROOT}/src/core/parser.cpp
    ${ORIP_ROOT}/src/core/session_manager.cpp
    ${ORIP_ROOT}/src/protocols/astm_f3411.cpp
    ${ORIP_ROOT}/src/utils/bit_reader.cpp
)

# JNI wrapper
add_library(orip_jni SHARED
    orip_jni.cpp
    ${ORIP_SOURCES}
)

target_include_directories(orip_jni PRIVATE
    ${ORIP_ROOT}/include
)

# Link with Android log library
find_library(log-lib log)
target_link_libraries(orip_jni ${log-lib})

# Optimize for size in release builds
target_compile_options(orip_jni PRIVATE
    $<$<CONFIG:Release>:-Os -fvisibility=hidden>
)
