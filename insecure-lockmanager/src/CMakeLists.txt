# HashTable
add_library(hashtable lockmanager/hashtable.cpp)
target_include_directories(hashtable PUBLIC "${LockManager_SOURCE_DIR}/include" "${LockManager_SOURCE_DIR}/include/lockmanager")

# Transaction
add_library(transaction lockmanager/transaction.cpp lockmanager/lock.cpp)
target_include_directories(transaction PUBLIC "${LockManager_SOURCE_DIR}/include" "${LockManager_SOURCE_DIR}/include/lockmanager")
target_link_libraries(transaction PUBLIC hashtable)

# Lock
add_library(lock lockmanager/lock.cpp)
target_include_directories(lock PUBLIC "${LockManager_SOURCE_DIR}/include" "${LockManager_SOURCE_DIR}/include/lockmanager")

set(LOCK_MANAGER_INCLUDE_PATH "${LockManager_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST 
    ${LOCK_MANAGER_INCLUDE_PATH}/lockmanager.h
    ${LOCK_MANAGER_INCLUDE_PATH}/lock.h
    ${LOCK_MANAGER_INCLUDE_PATH}/transaction.h
    ${LOCK_MANAGER_INCLUDE_PATH}/hashtable.h
    ${LockManager_SOURCE_DIR}/include/common.h
  )

set(SRCS lockmanager/lockmanager.cpp lockmanager/transaction.cpp lockmanager/lock.cpp lockmanager/hashtable.cpp ${HEADER_LIST})
add_library(lckMgr SHARED ${SRCS})

# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(TrustDBle::lckMgr ALIAS lckMgr)

# We need this directory, and users of our library will need it too
target_include_directories(lckMgr PUBLIC ../include ../include/lockmanager ${FETCHCONTENT_BASE_DIR}/spdlog-src/include/ ../build/src ${SGX_INCLUDE_DIR})

# add_dependencies(lckMgr enclave-sign)

# All users of this library will need at least C++17
target_compile_features(lckMgr PUBLIC cxx_std_17)

# Help IDEs to find header files easier
target_sources(lckMgr INTERFACE "$<BUILD_INTERFACE:${HEADERLIST}>")

add_subdirectory(proto)
add_subdirectory(client)
add_subdirectory(server)