cmake_minimum_required(VERSION 3.16...3.20)

project(
  LockManager
  VERSION 0.1
  DESCRIPTION "LockManager for TrustDBle"
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

# Help cmake find our custom cmake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Include required cmake modules
include(trustdble_configure)
include(FetchContent)

find_package(Threads REQUIRED)

# Concurrent Hashmap
FetchContent_Declare(
    libcuckoo
    GIT_REPOSITORY https://github.com/efficient/libcuckoo.git
    GIT_TAG        v0.3)
FetchContent_MakeAvailable(libcuckoo)

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

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  # Docs only available if this is the main app
  find_package(Doxygen)
  if(Doxygen_FOUND)
    add_subdirectory(docs)
  else()
    message(STATUS "Doxygen not found, not building docs")
  endif()

  # Tests only available if this is the main app
  set(BUILD_TESTING ON) # required since it is set to Off by grpc dependency
  include(CTest)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.10.0
  )
  set(CMAKE_POLICY_DEFAULT_CMP0048 NEW) # Required since googleTest uses an older cmake version
  FetchContent_MakeAvailable(googletest)
endif()

message("Value of BUILD_TESTING")
message(${BUILD_TESTING})

# Testing only available if this is the main app
# Emergency override MyProjectName_BUILD_TESTING provided as well
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR LockManager_BUILD_TESTING)
  AND BUILD_TESTING)
  message("WITH TESTS!!!!!!!!!!!!!!!!!!1")
 include(GoogleTest)
 add_subdirectory(tests)
else()
  message("WITHOUT TESTS!!!!!!!!!!!!!!!!!!")
endif()