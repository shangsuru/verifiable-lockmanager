# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

# Optionally glob, but only for CMake 3.12 or later:
#file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${TrustdbleStubAdapter_SOURCE_DIR}/include/adapter_stub/*.h")
set(LOCK_MANAGER_INCLUDE_PATH "${LockManager_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST 
    ${LOCK_MANAGER_INCLUDE_PATH}/lockmanager.h
    ${LOCK_MANAGER_INCLUDE_PATH}/lock.h
    ${LOCK_MANAGER_INCLUDE_PATH}/transaction.h
  )

# Make an automatic library - will be static or dynamic based on user setting
add_library(lckMgr lockmanager.cpp transaction.cpp lock.cpp ${HEADER_LIST})
# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(TrustDBle::lckMgr ALIAS lckMgr)

# We need this directory, and users of our library will need it too
target_include_directories(lckMgr PUBLIC ../../include/lockmanager ${FETCHCONTENT_BASE_DIR}/libcuckoo-src) 

# All users of this library will need at least C++17
target_compile_features(lckMgr PUBLIC cxx_std_17)

# Help IDEs to find header files easier
target_sources(lckMgr INTERFACE "$<BUILD_INTERFACE:${HEADERLIST}>")
