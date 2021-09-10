add_executable(clientMain client_main.cpp)
target_link_libraries(clientMain lckMgrClient)

add_executable(serverMain server_main.cpp)
target_link_libraries(serverMain lckMgrServer)
