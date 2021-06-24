#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <iostream>

#include "lockmanager.grpc.pb.h"
#include "lockmanager.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

/**
 * The LockingServiceImpl resembles the server side for the gRPC API of the lock
 * manager. It takes requests to acquire and release locks from the client and
 * forwards them to the LockManager class.
 */
class LockingServiceImpl final : public LockingService::Service {
 public:
  auto LockExclusive(ServerContext* context, const LockRequest* request,
                     LockResponse* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int row_id = request->row_id();

    LockManager::lock(transaction_id, row_id, Lock::LockMode::kExclusive);

    response->set_successful(true);
    response->set_signature("signature");  // TODO

    return Status::OK;
  }

  auto LockShared(ServerContext* context, const LockRequest* request,
                  LockResponse* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int row_id = request->row_id();

    LockManager::lock(transaction_id, row_id, Lock::LockMode::kShared);

    response->set_successful(true);
    response->set_signature("signature");  // TODO

    return Status::OK;
  }

  auto Unlock(ServerContext* context, const LockRequest* request,
              LockResponse* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int row_id = request->row_id();

    LockManager::unlock(transaction_id, row_id);

    response->set_successful(true);
    response->set_signature("signature");  // TODO
    return Status::OK;
  }

 private:
  LockManager lockManager_;
};

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