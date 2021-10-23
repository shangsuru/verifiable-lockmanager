#include "client.h"

LockingServiceClient::LockingServiceClient(
    const std::shared_ptr<Channel> &channel) {
  stub_ = LockingService::NewStub(channel);
}

auto LockingServiceClient::registerTransaction(unsigned int transactionId,
                                               unsigned int lockBudget)
    -> bool {
  RegistrationRequest registration;
  registration.set_transaction_id(transactionId);

  RegistrationResponse acceptance;
  ClientContext context;

  Status status =
      stub_->RegisterTransaction(&context, registration, &acceptance);

  if (status.ok()) {
    spdlog::info(
        "Registered transaction (TXID: " + std::to_string(transactionId) + ")");
  }

  return status.ok();
};

auto LockingServiceClient::requestSharedLock(unsigned int transactionId,
                                             unsigned int rowId) -> bool {
  spdlog::info(
      "Requesting shared lock (TXID: " + std::to_string(transactionId) +
      ", RID: " + std::to_string(rowId) + ")");
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->LockShared(&context, request, &response);

  if (status.ok()) {
    spdlog::info(
        "Acquired shared lock (TXID: " + std::to_string(transactionId) +
        ", RID: " + std::to_string(rowId) + ")");
  }

  return status.ok();
}

auto LockingServiceClient::requestExclusiveLock(unsigned int transactionId,
                                                unsigned int rowId) -> bool {
  spdlog::info(
      "Requesting exclusive lock (TXID: " + std::to_string(transactionId) +
      ", RID: " + std::to_string(rowId) + ")");
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->LockExclusive(&context, request, &response);

  if (status.ok()) {
    spdlog::info(
        "Acquired exclusive lock (TXID: " + std::to_string(transactionId) +
        ", RID: " + std::to_string(rowId) + ")");
  }

  return status.ok();
}

auto LockingServiceClient::requestUnlock(unsigned int transactionId,
                                         unsigned int rowId) -> bool {
  spdlog::info(
      "Requesting to release a lock (TXID: " + std::to_string(transactionId) +
      ", RID: " + std::to_string(rowId) + ")");
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->Unlock(&context, request, &response);

  return status.ok();
}