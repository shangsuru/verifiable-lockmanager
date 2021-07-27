# Intel SGX
find_package(SGX REQUIRED)

set(E_SRCS enclave/Enclave.cpp)
set(T_SCRS "")
set(EDL_SEARCH_PATHS enclave)

add_trusted_library(trusted_lib SRCS ${T_SRCS} EDL enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
add_enclave_library(enclave SRCS ${E_SRCS} TRUSTED_LIBS trusted_lib EDL enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
enclave_sign(enclave KEY enclave/Enclave_private_test.pem CONFIG enclave/Enclave.config.xml)
target_include_directories(enclave PUBLIC ../include/enclave ../include/)


set(LOCK_MANAGER_INCLUDE_PATH "${LockManager_SOURCE_DIR}/include/lockmanager")
set(HEADER_LIST 
    ${LOCK_MANAGER_INCLUDE_PATH}/lockmanager.h
    ${LOCK_MANAGER_INCLUDE_PATH}/lock.h
    ${LOCK_MANAGER_INCLUDE_PATH}/transaction.h
    ${LOCK_MANAGER_INCLUDE_PATH}/ErrorSupport.h
    ${LOCK_MANAGER_INCLUDE_PATH}/FileIO.h
    ${LockManager_SOURCE_DIR}/include/Base64Encoding.h
  )

set(SRCS lockmanager/lockmanager.cpp lockmanager/ErrorSupport.cpp lockmanager/FileIO.cpp Base64Encoding.cpp lockmanager/lock.cpp lockmanager/transaction.cpp ${HEADER_LIST})
add_untrusted_library(lckMgr SHARED SRCS ${SRCS} EDL enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})

# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(TrustDBle::lckMgr ALIAS lckMgr)

# We need this directory, and users of our library will need it too
target_include_directories(lckMgr PUBLIC ../include ../include/lockmanager ${FETCHCONTENT_BASE_DIR}/libcuckoo-src ../build/src ${SGX_INCLUDE_DIR})

# add_dependencies(lckMgr enclave-sign)

# All users of this library will need at least C++17
target_compile_features(lckMgr PUBLIC cxx_std_17)

# Help IDEs to find header files easier
target_sources(lckMgr INTERFACE "$<BUILD_INTERFACE:${HEADERLIST}>")

add_subdirectory(proto)
add_subdirectory(client)
add_subdirectory(server)