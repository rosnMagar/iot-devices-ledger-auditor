cmake_minimum_required(VERSION 3.16)
project(storage_core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

# cpp-httplib runs each connection on its own thread.
find_package(Threads REQUIRED)

add_executable(storage-core src/main.cpp src/chain.cpp src/storage.cpp src/server.cpp)
target_include_directories(storage-core PRIVATE include third_party)
target_link_libraries(storage-core PRIVATE OpenSSL::Crypto Threads::Threads)

# Tests are ON for local dev and CI, but the production Docker image builds with
# -DSTORAGE_CORE_BUILD_TESTS=OFF so it never needs the tests/ sources.
option(STORAGE_CORE_BUILD_TESTS "Build storage-core unit tests" ON)

if(STORAGE_CORE_BUILD_TESTS)
    enable_testing()

    add_executable(test-chain tests/test_chain.cpp src/chain.cpp)
    target_include_directories(test-chain PRIVATE include third_party)
    target_link_libraries(test-chain PRIVATE OpenSSL::Crypto)

    add_test(NAME chain-unit-tests COMMAND test-chain)

    add_executable(test-storage tests/test_storage.cpp src/storage.cpp src/chain.cpp)
    target_include_directories(test-storage PRIVATE include third_party)
    target_link_libraries(test-storage PRIVATE OpenSSL::Crypto)

    add_test(NAME storage-unit-tests COMMAND test-storage)
endif()
