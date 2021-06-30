#include "server.h"

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  LockingServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on port: " << server_address << std::endl;

  server->Wait();
}

auto main(int argc, char** argv) -> int {
  RunServer();
  return 0;
}