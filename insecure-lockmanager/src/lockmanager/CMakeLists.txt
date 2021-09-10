set(LOCK_MANAGER_INCLUDE_PATH "${InsecureLockManager_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST 
    ${LOCK_MANAGER_INCLUDE_PATH}/lockmanager.h
    ${LOCK_MANAGER_INCLUDE_PATH}/lock.h
    ${LOCK_MANAGER_INCLUDE_PATH}/transaction.h
)

set(SRCS 
    transaction.cpp
    lock.cpp
)

add_library(lckMgr lockmanager.cpp ${SRCS} ${HEADER_LIST})

# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(TrustDBle::lckMgr ALIAS lckMgr)

# We need this directory, and users of our library will need it too
target_include_directories(lckMgr PUBLIC ${InsecureLockManager_SOURCE_DIR}/include/ ${InsecureLockManager_SOURCE_DIR}/include/lockmanager ${FETCHCONTENT_BASE_DIR}/libcuckoo-src ${FETCHCONTENT_BASE_DIR}/spdlog-src/include/ ${InsecureLockManager_SOURCE_DIR}/build/src)

# All users of this library will need at least C++17
target_compile_features(lckMgr PUBLIC cxx_std_17)

# Help IDEs to find header files easier
target_sources(lckMgr INTERFACE "$<BUILD_INTERFACE:${HEADERLIST}>")