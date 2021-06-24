cmake_minimum_required(VERSION 3.16...3.20)

add_executable(Client client.cpp)
target_link_libraries(Client
    lm_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
target_include_directories(Client PRIVATE "${CMAKE_BINARY_DIR}")