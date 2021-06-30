cmake_minimum_required(VERSION 3.16...3.20)

add_library(Client client.cpp)
target_link_libraries(Client
    lm_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
target_include_directories(Client PUBLIC "${CMAKE_BINARY_DIR}" PUBLIC "${CMAKE_SOURCE_DIR}/include/client")

add_executable(ClientMain main.cpp)
target_link_libraries(ClientMain Client)
