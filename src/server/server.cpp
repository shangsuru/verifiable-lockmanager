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
  /**
   * Unpacks the LockRequest by a client to acquire the respective exclusive
   * lock.
   *
   * @param context contains metadata about the request
   * @param request containing transaction ID and row ID of the client request,
   *                that identify client and the row it wants a lock on
   * @param response contains if the lock was acquired successfully and if it
   *                 was a signature of the lock
   * @return the status code of the RPC call (OK or a specific error code)
   */
  auto LockExclusive(ServerContext* context, const LockRequest* request,
                     LockResponse* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int row_id = request->row_id();

    LockManager::lock(transaction_id, row_id, Lock::LockMode::kExclusive);

    response->set_successful(true);
    response->set_signature("signature");  // TODO

    return Status::OK;
  }

  /**
   * Unpacks the LockRequest by a client to acquire the respective shared
   * lock.
   *
   * @param context contains metadata about the request
   * @param request containing transaction ID and row ID of the client request,
   *                that identify client and the row it wants a lock on
   * @param response contains if the lock was acquired successfully and if it
   *                 was a signature of the lock
   * @return the status code of the RPC call (OK or a specific error code)
   */
  auto LockShared(ServerContext* context, const LockRequest* request,
                  LockResponse* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int row_id = request->row_id();

    LockManager::lock(transaction_id, row_id, Lock::LockMode::kShared);

    response->set_successful(true);
    response->set_signature("signature");  // TODO

    return Status::OK;
  }

  /**
   * Unpacks the LockRequest by a client to release a lock he acquired
   * previously.
   *
   * @param context contains metadata about the request
   * @param request containing transaction ID and row ID of the client request,
   *                that identify client and the row it wants to unlock
   * @param response contains if the lock was released successfully as
   *                 acknowledgement
   * @return the status code of the RPC call (OK or a specific error code)
   */
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