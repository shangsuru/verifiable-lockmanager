cmake_minimum_required(VERSION 3.16...3.20)

project(
  LockManager
  VERSION 0.1
  DESCRIPTION "LockManager for TrustDBle"
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(FetchContent)

add_subdirectory(src)


if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
#### Doxygen #####
find_package(Doxygen)
  if(Doxygen_FOUND)
    add_subdirectory(docs)
  else()
    message(STATUS "Doxygen not found, not building docs")
  endif()

#### Testing #####
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