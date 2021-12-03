
# HashTable
add_library(hashtable hashtable.cpp)
target_include_directories(hashtable PUBLIC "${LockManager_SOURCE_DIR}/include")

# Transaction
add_library(transaction transaction.cpp lock.cpp)
target_include_directories(transaction PUBLIC "${LockManager_SOURCE_DIR}/include")

# Lock
add_library(lock lock.cpp)
target_include_directories(lock PUBLIC "${LockManager_SOURCE_DIR}/include")

# Intel SGX
find_package(SGX REQUIRED)

set(E_SRCS enclave/enclave.cpp enclave/lock_signatures.cpp base64-encoding.cpp transaction.cpp lock.cpp hashtable.cpp)
set(T_SCRS "")
set(EDL_SEARCH_PATHS enclave)

include_directories(${SGX_INCLUDE_DIR} ../include)

add_trusted_library(trusted_lib SRCS ${T_SRCS} EDL enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
add_enclave_library(enclave SRCS ${E_SRCS} TRUSTED_LIBS trusted_lib EDL enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
enclave_sign(enclave KEY enclave/Enclave_private_test.pem CONFIG enclave/enclave.config.xml)
target_include_directories(enclave PUBLIC ../include/enclave ../include/)

set(LOCK_MANAGER_INCLUDE_PATH "${LockManager_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST 
    ${LOCK_MANAGER_INCLUDE_PATH}/lockmanager.h
    ${LOCK_MANAGER_INCLUDE_PATH}/errors.h
    ${LOCK_MANAGER_INCLUDE_PATH}/files.h
    ${LockManager_SOURCE_DIR}/include/base64-encoding.h
    ${LockManager_SOURCE_DIR}/include/common.h
    ${LockManager_SOURCE_DIR}/include/lock.h
    ${LockManager_SOURCE_DIR}/include/transaction.h
    ${LockManager_SOURCE_DIR}/include/hashtable.h
  )
set(LCKMGR_SRCS
  lockmanager/lockmanager.cpp 
  lockmanager/errors.cpp 
  lockmanager/files.cpp 
  lockmanager/ocalls.cpp 
  base64-encoding.cpp
  lock.cpp
  transaction.cpp
  hashtable.cpp
)
set(SRCS ${LCKMGR_SRCS} ${HEADER_LIST})
add_untrusted_library(lckMgr SHARED SRCS ${SRCS} EDL enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})

# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(TrustDBle::lckMgr ALIAS lckMgr)

# We need this directory, and users of our library will need it too
target_include_directories(lckMgr PUBLIC ../include ../include/lockmanager ../include/enclave ${FETCHCONTENT_BASE_DIR}/spdlog-src/include/ ../build/src ${SGX_INCLUDE_DIR})

# add_dependencies(lckMgr enclave-sign)

# All users of this library will need at least C++17
target_compile_features(lckMgr PUBLIC cxx_std_17)

# Help IDEs to find header files easier
target_sources(lckMgr INTERFACE "$<BUILD_INTERFACE:${HEADERLIST}>")

add_subdirectory(proto)
add_subdirectory(client)
add_subdirectory(server)