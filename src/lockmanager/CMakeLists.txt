set(LOCK_MANAGER_LIB "${CMAKE_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST ${LOCK_MANAGER_LIB}/lockmanager.h ${LOCK_MANAGER_LIB}/lock.h ${LOCK_MANAGER_LIB}/transaction.h)

add_library(LockManager lockmanager.cpp ${HEADER_LIST})
target_include_directories(LockManager PUBLIC ${LOCK_MANAGER_LIB})
