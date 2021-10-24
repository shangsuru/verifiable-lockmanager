set(SERVER_ADDRESS "0.0.0.0:50051")
configure_file(
  "${PROJECT_SOURCE_DIR}/include/client/client.h.in"
  "${PROJECT_SOURCE_DIR}/include/client/client.h"
)
configure_file(
  "${PROJECT_SOURCE_DIR}/include/server/server.h.in"
  "${PROJECT_SOURCE_DIR}/include/server/server.h"
)

add_executable(clientMain client_main.cpp)
target_link_libraries(clientMain lckMgrClient)

add_executable(serverMain server_main.cpp)
target_link_libraries(serverMain lckMgrServer)
