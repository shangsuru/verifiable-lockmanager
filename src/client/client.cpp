#include <grpcpp/grpcpp.h>

#include <iostream>
#include <string_view>

#include "lockmanager.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

/**
 * The LockingServiceClient is the gRPC client for the lock manager.
 * It sends request to the gRPC server to acquire and release locks.
 */
class LockingServiceClient {
 public:
  /**
   * Initializes a new client stub, which provides the functions for
   * the gRPC calls to the server.
   *
   * @param channel the abstraction of a connection to the remote server
   */
  LockingServiceClient(const std::shared_ptr<Channel> &channel) {
    stub_ = LockingService::NewStub(channel);
  }

  /**
   * Registers the transaction at the lock manager by setting the lock budget.
   *
   * @param transactionId identifies the transaction
   * @param lockBudget the maximum number of locks the transaction can acquire
   */
  void registerTransaction(unsigned int transactionId,
                           unsigned int lockBudget) {
    Registration registration;
    registration.set_transaction_id(transactionId);
    registration.set_lock_budget(lockBudget);

    Acceptance acceptance;
    ClientContext context;

    Status status =
        stub_->RegisterTransaction(&context, registration, &acceptance);

    if (status.ok()) {
      std::cout << "Registered this transaction" << std::endl;
    } else {
      std::cout << "Registration failed with error code " << status.error_code()
                << ": " << status.error_message() << std::endl;
    }
  };

  /**
   * Requests a shared lock for read-only access to a row.
   *
   * @param transactionId identifies the transaction that makes the request
   * @param rowId identifies the row, the transaction wants to access
   */
  void requestSharedLock(unsigned int transactionId, unsigned int rowId) {
    LockRequest request;
    request.set_transaction_id(transactionId);
    request.set_row_id(rowId);

    LockResponse response;
    ClientContext context;

    Status status = stub_->LockShared(&context, request, &response);

    if (status.ok()) {
      std::cout << "Signature: " << response.signature() << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
  }

  /**
   * Requests an exclusive lock for sole write access to a row.
   *
   * @param transactionId identifies the transaction that makes the request
   * @param rowId identifies the row, the transaction wants to access
   */
  void requestExclusiveLock(unsigned int transactionId, unsigned int rowId) {
    LockRequest request;
    request.set_transaction_id(transactionId);
    request.set_row_id(rowId);

    LockResponse response;
    ClientContext context;

    Status status = stub_->LockExclusive(&context, request, &response);

    if (status.ok()) {
      std::cout << "Signature: " << response.signature() << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
  }

  /**
   * Requests to release a lock acquired by the transaction.
   *
   * @param transactionId identifies the transaction that makes the request
   * @param rowId identifies the row, the transaction wants to unlock
   */
  void requestUnlock(unsigned int transactionId, unsigned int rowId) {
    LockRequest request;
    request.set_transaction_id(transactionId);
    request.set_row_id(rowId);

    LockResponse response;
    ClientContext context;

    Status status = stub_->Unlock(&context, request, &response);

    if (status.ok()) {
      std::cout << "fSignature: " << response.signature() << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
  }

 private:
  std::unique_ptr<LockingService::Stub> stub_;
};

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  unsigned int transaction_id = 1;
  const unsigned int default_lock_budget = 10;
  unsigned int row_id = 2;

  client.registerTransaction(transaction_id, default_lock_budget);
  client.requestSharedLock(transaction_id, row_id);
}

auto main() -> int {
  std::cout << "Starting client" << std::endl;
  RunClient();
  return 0;
}