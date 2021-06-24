cmake_minimum_required(VERSION 3.16...3.20)

project(
  LockManager
  VERSION 0.1
  DESCRIPTION "LockManager for TrustDBle"
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(trustdble_configure)
include(FetchContent)

add_subdirectory(src)

find_package(Threads REQUIRED)

FetchContent_Declare(
    grpc
    GIT_REPOSITORY https://github.com/grpc/grpc.git
    GIT_TAG        v1.38.0)
FetchContent_MakeAvailable(grpc)

set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

# Proto file
get_filename_component(lm_proto "src/proto/lockmanager.proto" ABSOLUTE)
get_filename_component(lm_proto_path "${lm_proto}" PATH)

# Generated sources
set(lm_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.pb.cc")
set(lm_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.pb.h")
set(lm_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.grpc.pb.cc")
set(lm_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.grpc.pb.h")
add_custom_command(
      OUTPUT "${lm_proto_srcs}" "${lm_proto_hdrs}" "${lm_grpc_srcs}" "${lm_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${lm_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${lm_proto}"
      DEPENDS "${lm_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# lm_grpc_proto
add_library(lm_grpc_proto
  ${lm_grpc_srcs}
  ${lm_grpc_hdrs}
  ${lm_proto_srcs}
  ${lm_proto_hdrs})
target_link_libraries(lm_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
# Doxygen
find_package(Doxygen)
  if(Doxygen_FOUND)
    add_subdirectory(docs)
  else()
    message(STATUS "Doxygen not found, not building docs")
  endif()

# Testing
include(CTest)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.10.0
  )
  set(CMAKE_POLICY_DEFAULT_CMP0048 NEW) # Required since googleTest uses an older cmake version
  FetchContent_MakeAvailable(googletest)
endif()

# Testing only available if this is the main app
# Emergency override MyProjectName_BUILD_TESTING provided as well
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR LockManager_BUILD_TESTING)
   AND BUILD_TESTING)
  include(GoogleTest)
  add_subdirectory(tests)
endif()