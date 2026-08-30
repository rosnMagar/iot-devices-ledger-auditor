cmake_minimum_required(VERSION 3.16)
project(storage_core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

# cpp-httplib runs each connection on its own thread.
find_package(Threads REQUIRED)

# IXWebSocket, vendored (BSD-3-Clause). Compiled from source rather than via
# upstream's CMakeLists so the options stay ours: neither IXWEBSOCKET_USE_TLS nor
# IXWEBSOCKET_USE_ZLIB is defined, which is what keeps it free of OpenSSL and
# zlib. See third_party/ixwebsocket/VENDORING.md before changing that.
file(GLOB IXWEBSOCKET_SOURCES third_party/ixwebsocket/*.cpp)
add_library(ixwebsocket STATIC ${IXWEBSOCKET_SOURCES})
target_include_directories(ixwebsocket PUBLIC third_party)
target_link_libraries(ixwebsocket PUBLIC Threads::Threads)
# Upstream's own warnings are not our problem and would drown ours.
target_compile_options(ixwebsocket PRIVATE -w)

add_executable(storage-core src/main.cpp src/chain.cpp src/storage.cpp src/server.cpp src/writer.cpp)
target_include_directories(storage-core PRIVATE include third_party)
target_link_libraries(storage-core PRIVATE OpenSSL::Crypto Threads::Threads ixwebsocket)

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

    # Integration tests: these start a real server on a real socket, so unlike
    # the unit tests above they need Threads.
    add_executable(test-server tests/test_server.cpp src/server.cpp src/chain.cpp src/storage.cpp src/writer.cpp)
    target_include_directories(test-server PRIVATE include third_party)
    target_link_libraries(test-server PRIVATE OpenSSL::Crypto Threads::Threads)

    add_test(NAME server-integration-tests COMMAND test-server)

    # IOT-45 build probe: proves the vendored IXWebSocket compiles, links and
    # binds. Replaced by the real WebSocket tests in IOT-29.
    add_executable(ws-probe tests/ws_probe.cpp)
    target_link_libraries(ws-probe PRIVATE ixwebsocket)

    add_test(NAME ws-probe COMMAND ws-probe)

    add_executable(test-writer tests/test_writer.cpp src/writer.cpp src/chain.cpp)
    target_include_directories(test-writer PRIVATE include third_party)
    target_link_libraries(test-writer PRIVATE OpenSSL::Crypto Threads::Threads)

    add_test(NAME writer-unit-tests COMMAND test-writer)
endif()
