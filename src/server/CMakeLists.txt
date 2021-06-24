cmake_minimum_required(VERSION 3.16...3.20)

add_executable(Server server.cpp)
target_link_libraries(Server
    lm_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    LockManager)

target_include_directories(Server 
                            PRIVATE "${CMAKE_BINARY_DIR}" 
                            PRIVATE "${CMAKE_SOURCE_DIR}/include/lockmanager")
