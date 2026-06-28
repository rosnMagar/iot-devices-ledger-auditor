cmake_minimum_required(VERSION 3.16)
project(storage_core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(storage-core src/main.cpp)
target_include_directories(storage-core PRIVATE include third_party)
