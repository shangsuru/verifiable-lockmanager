cmake_minimum_required(VERSION 3.16...3.20)

project(
  InsecureLockManager
  VERSION 0.1
  DESCRIPTION "Insecure Lock　Manager"
  LANGUAGES CXX C)

set (CMAKE_CXX_STANDARD 17)

# Include required cmake modules
include(FetchContent)
find_package(Threads REQUIRED)

# Concurrent Hashmap
FetchContent_Declare(
    libcuckoo
    GIT_REPOSITORY https://github.com/efficient/libcuckoo.git
    GIT_TAG        v0.3)
FetchContent_MakeAvailable(libcuckoo)

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

add_subdirectory(src)
add_subdirectory(apps)