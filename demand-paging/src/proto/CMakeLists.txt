# Proto file
get_filename_component(lm_proto "lockmanager.proto" ABSOLUTE)
get_filename_component(lm_proto_path "${lm_proto}" PATH)

# Generated sources
set(lm_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.pb.cc")
set(lm_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.pb.h")
set(lm_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.grpc.pb.cc")
set(lm_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/lockmanager.grpc.pb.h")

add_custom_command(
      OUTPUT "${lm_proto_srcs}" "${lm_proto_hdrs}" "${lm_grpc_srcs}" "${lm_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${lm_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${lm_proto}"
      DEPENDS "${lm_proto}")

# lm_grpc_proto
add_library(lm_grpc_proto
  ${lm_grpc_srcs}
  ${lm_grpc_hdrs}
  ${lm_proto_srcs}
  ${lm_proto_hdrs})
target_link_libraries(lm_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


# Include generated *.pb.h files
target_include_directories(lm_grpc_proto PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")