#include "client.h"

LockingServiceClient::LockingServiceClient(
    const std::shared_ptr<Channel> &channel) {
  stub_ = LockingService::NewStub(channel);
}

auto LockingServiceClient::registerTransaction(unsigned int transactionId,
                                               unsigned int lockBudget)
    -> bool {
  if (transactionId == 0) {
    spdlog::error("Cannot register a transaction with ID 0");
    return false;
  }

  RegistrationRequest registration;
  registration.set_transaction_id(transactionId);
  registration.set_lock_budget(lockBudget);

  RegistrationResponse acceptance;
  ClientContext context;

  Status status =
      stub_->RegisterTransaction(&context, registration, &acceptance);

  return status.ok();
};

auto LockingServiceClient::requestSharedLock(unsigned int transactionId,
                                             unsigned int rowId)
    -> std::string {
  if (transactionId == 0 || rowId == 0) {
    spdlog::error("Cannot acquire lock for TXID 0 or RID 0");
    return "";
  }

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
    spdlog::info("Received signature: " + response.signature());
    return response.signature();
  }

  spdlog::error(
      "Acquiring shared lock failed (TXID: " + std::to_string(transactionId) +
      ", RID: " + std::to_string(rowId) + ")");
  return "";
}

auto LockingServiceClient::requestExclusiveLock(unsigned int transactionId,
                                                unsigned int rowId)
    -> std::string {
  if (transactionId == 0 || rowId == 0) {
    spdlog::error("Cannot acquire lock for TXID 0 or RID 0");
    return "";
  }

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
    spdlog::info("Received signature: " + response.signature());
    return response.signature();
  }

  spdlog::error("Acquiring exclusive lock failed (TXID: " +
                std::to_string(transactionId) +
                ", RID: " + std::to_string(rowId) + ")");
  return "";
}

auto LockingServiceClient::requestUnlock(unsigned int transactionId,
                                         unsigned int rowId) -> bool {
  if (transactionId == 0 || rowId == 0) {
    spdlog::error("Cannot unlock for TXID 0 or RID 0");
    return false;
  }

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