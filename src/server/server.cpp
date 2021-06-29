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
   * Registers the transaction at the lock manager prior to being able to
   * acquire any locks, so that the lock manager can now the transaction's lock
   * budget.
   *
   * @param context contains metadata about the request
   * @param request containing transaction ID and lock budget
   * @param response contains if the registration was successful
   * @return the status code of the RPC call (OK or a specific error code)
   */
  auto RegisterTransaction(ServerContext* context, const Registration* request,
                           Acceptance* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int lock_budget = request->lock_budget();

    try {
      lockManager_.registerTransaction(transaction_id, lock_budget);
    } catch (const std::domain_error& e) {
      return Status::CANCELLED;
    }

    return Status::OK;
  };

  /**
   * Unpacks the LockRequest by a client to acquire the respective exclusive
   * lock.
   *
   * @param context contains metadata about the request
   * @param request containing transaction ID and row ID of the client request,
   *                that identify client and the row it wants a lock on
   * @param response contains if the lock was acquired successfully and if it
   *                 was, a signature of the lock
   * @return the status code of the RPC call (OK or a specific error code)
   */
  auto LockExclusive(ServerContext* context, const LockRequest* request,
                     LockResponse* response) -> Status override {
    unsigned int transaction_id = request->transaction_id();
    unsigned int row_id = request->row_id();

    std::string signature;
    try {
      signature =
          lockManager_.lock(transaction_id, row_id, Lock::LockMode::kExclusive);
    } catch (const std::domain_error& e) {
      return Status::CANCELLED;
    }

    response->set_signature(signature);
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

    std::string signature;
    try {
      signature =
          lockManager_.lock(transaction_id, row_id, Lock::LockMode::kShared);
    } catch (const std::domain_error& e) {
      return Status::CANCELLED;
    }

    response->set_signature(signature);
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

    try {
      lockManager_.unlock(transaction_id, row_id);
    } catch (const std::domain_error& e) {
      return Status::CANCELLED;
    }

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