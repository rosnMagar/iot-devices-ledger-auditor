cmake_minimum_required(VERSION 3.16)
project(storage_core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

add_executable(storage-core src/main.cpp src/chain.cpp)
target_include_directories(storage-core PRIVATE include third_party)
target_link_libraries(storage-core PRIVATE OpenSSL::Crypto)

# Tests are ON for local dev and CI, but the production Docker image builds with
# -DSTORAGE_CORE_BUILD_TESTS=OFF so it never needs the tests/ sources.
option(STORAGE_CORE_BUILD_TESTS "Build storage-core unit tests" ON)

if(STORAGE_CORE_BUILD_TESTS)
    enable_testing()

    add_executable(test-chain tests/test_chain.cpp src/chain.cpp)
    target_include_directories(test-chain PRIVATE include third_party)
    target_link_libraries(test-chain PRIVATE OpenSSL::Crypto)

    add_test(NAME chain-unit-tests COMMAND test-chain)
endif()
