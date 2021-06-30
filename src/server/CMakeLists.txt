cmake_minimum_required(VERSION 3.16...3.20)

add_library(Server server.cpp)
target_link_libraries(Server
    lm_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    LockManager)

target_include_directories(Server 
                            PUBLIC "${CMAKE_BINARY_DIR}" 
                            PUBLIC "${CMAKE_SOURCE_DIR}/include/lockmanager"
                            PUBLIC "${CMAKE_SOURCE_DIR}/include/server")

add_executable(ServerMain main.cpp)
target_link_libraries(ServerMain Server)
