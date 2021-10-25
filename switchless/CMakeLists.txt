cmake_minimum_required(VERSION 2.8)
project(sign_enclave)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(SGX REQUIRED)

set(E_SRCS Enclave/Enclave.cpp)


include_directories(${SGX_INCLUDE_DIR} App Enclave)
set(EDL_SEARCH_PATHS enclave)

add_trusted_library(trusted_lib SRCS ${T_SRCS} EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
add_enclave_library(enclave SRCS ${E_SRCS} TRUSTED_LIBS trusted_lib EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
enclave_sign(enclave KEY Enclave/Enclave_private_test.pem CONFIG Enclave/Enclave.config.xml)

set(SRCS App/App.cpp)
add_untrusted_executable(sign-enclave SRCS ${SRCS} EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
add_dependencies(sign-enclave enclave-sign)