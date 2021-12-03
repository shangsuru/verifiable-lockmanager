# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

# Optionally glob, but only for CMake 3.12 or later:
#file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${TrustdbleStubAdapter_SOURCE_DIR}/include/adapter_stub/*.h")
set(HEADER_LIST 
  "${LockManager_SOURCE_DIR}/include/server/server.h"
  )

# Make an automatic library - will be static or dynamic based on user setting
add_library(lckMgrServer server.cpp ${HEADER_LIST})
# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(TrustDBle::lckMgrServer ALIAS lckMgrServer)

# We need this directory, and users of our library will need it too
target_include_directories(lckMgrServer PUBLIC ../../include/server ${FETCHCONTENT_BASE_DIR}/spdlog-src/include/)

target_link_libraries(lckMgrServer
    lm_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    lckMgr
)

# All users of this library will need at least C++17
target_compile_features(lckMgrServer PUBLIC cxx_std_17)

# Help IDEs to find header files easier
target_sources(lckMgrServer INTERFACE "$<BUILD_INTERFACE:${HEADERLIST}>")