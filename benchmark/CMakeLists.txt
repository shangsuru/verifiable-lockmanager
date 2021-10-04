cmake_minimum_required(VERSION 3.16...3.20)

project(
  BenchMark
  VERSION 0.1
  DESCRIPTION "Benchmarks for LockManager"
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

# Help cmake find our custom cmake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake)

# Include required cmake modules
include(trustdble_configure)
include(FetchContent)

find_package(Threads REQUIRED)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.9.0)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    grpc
    GIT_REPOSITORY https://github.com/grpc/grpc.git
    GIT_TAG        v1.38.0)
FetchContent_MakeAvailable(grpc)
# Since FetchContent uses add_subdirectory under the hood, we can use
# the grpc targets directly from this build.
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark lckMgrClient)
target_include_directories(benchmark PUBLIC include/)

add_subdirectory(client)
add_subdirectory(proto)