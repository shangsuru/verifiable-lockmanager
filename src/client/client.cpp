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
  LockingServiceClient(const std::shared_ptr<Channel> &channel) {
    stub_ = LockingService::NewStub(channel);
  }

  void requestSharedLock(int transactionId, int rowId) {
    LockRequest request;
    request.set_transaction_id(transactionId);
    request.set_row_id(rowId);

    LockResponse response;
    ClientContext context;

    Status status = stub_->LockShared(&context, request, &response);

    if (status.ok()) {
      std::cout << "Received response, signature: " << response.signature()
                << " is set: " << response.successful() << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
  }

  void requestExclusiveLock(int transactionId, int rowId) {
    LockRequest request;
    request.set_transaction_id(transactionId);
    request.set_row_id(rowId);

    LockResponse response;
    ClientContext context;

    Status status = stub_->LockExclusive(&context, request, &response);

    if (status.ok()) {
      std::cout << "Received response, signature: " << response.signature()
                << " is set: " << response.successful() << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
  }

  void requestUnlock(int transactionId, int rowId) {
    LockRequest request;
    request.set_transaction_id(transactionId);
    request.set_row_id(rowId);

    LockResponse response;
    ClientContext context;

    Status status = stub_->Unlock(&context, request, &response);

    if (status.ok()) {
      std::cout << "Received response, signature: " << response.signature()
                << " is set: " << response.successful() << std::endl;
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
  client.requestSharedLock(1, 2);
}

auto main() -> int {
  std::cout << "Starting client" << std::endl;
  RunClient();
  return 0;
}