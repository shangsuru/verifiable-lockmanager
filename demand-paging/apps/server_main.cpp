#include "server.h"

void RunServer() {
  LockingServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());

  server->Wait();
}

auto main(int argc, char** argv) -> int {
  spdlog::set_level(spdlog::level::info);
  RunServer();
  return 0;
}