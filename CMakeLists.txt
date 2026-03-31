cmake_minimum_required(VERSION 3.16)

project(KVStore
    VERSION 0.1.0
    DESCRIPTION "High-performance key-value store based on skip list"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KVSTORE_BUILD_TESTS "Build KVStore tests" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(kvstore_compile_options INTERFACE)
target_compile_features(kvstore_compile_options INTERFACE cxx_std_14)
target_compile_options(kvstore_compile_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
)

find_package(Threads REQUIRED)

add_library(kvstore_core STATIC
    src/Protocol.cpp
    src/Server.cpp
    src/WAL.cpp
    src/kvstore.cpp
)

target_include_directories(kvstore_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(kvstore_core
    PUBLIC
        kvstore_compile_options
        Threads::Threads
)

add_executable(kvstore_server
    src/server_main.cpp
)

target_link_libraries(kvstore_server
    PRIVATE
        kvstore_core
)

set_target_properties(kvstore_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

add_executable(kvstore_client
    src/client_main.cpp
)

target_link_libraries(kvstore_client
    PRIVATE
        kvstore_compile_options
        Threads::Threads
)

set_target_properties(kvstore_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

add_executable(kvstore_bench
    src/benchmark_main.cpp
)

target_link_libraries(kvstore_bench
    PRIVATE
        kvstore_compile_options
        Threads::Threads
)

set_target_properties(kvstore_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

add_executable(kvstore_compare_bench
    src/compare_benchmark_main.cpp
)

target_link_libraries(kvstore_compare_bench
    PRIVATE
        kvstore_core
)

set_target_properties(kvstore_compare_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

if(KVSTORE_BUILD_TESTS)
    enable_testing()

    add_executable(kvstore_tests
        tests/test_main.cpp
    )

    target_link_libraries(kvstore_tests
        PRIVATE
            kvstore_core
    )

    set_target_properties(kvstore_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    )

    add_test(NAME kvstore_smoke_test COMMAND $<TARGET_FILE:kvstore_tests>)
endif()
