set(LOCK_MANAGER_LIB "${CMAKE_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST ${LOCK_MANAGER_LIB}/lockmanager.h ${LOCK_MANAGER_LIB}/lock.h ${LOCK_MANAGER_LIB}/transaction.h)

add_library(Lock lock.cpp ${LOCK_MANAGER_LIB}/lock.h)
target_include_directories(Lock PRIVATE ${LOCK_MANAGER_LIB})

add_library(Transaction transaction.cpp ${LOCK_MANAGER_LIB}/transaction.h ${LOCK_MANAGER_LIB}/lock.h)
target_include_directories(Transaction PRIVATE ${LOCK_MANAGER_LIB} PUBLIC ${FETCHCONTENT_BASE_DIR}/libcuckoo-src)

add_library(LockManager lockmanager.cpp ${HEADER_LIST})
target_include_directories(LockManager PUBLIC ${LOCK_MANAGER_LIB} PUBLIC ${FETCHCONTENT_BASE_DIR}/libcuckoo-src)
target_link_libraries(LockManager PRIVATE Lock PRIVATE Transaction)
