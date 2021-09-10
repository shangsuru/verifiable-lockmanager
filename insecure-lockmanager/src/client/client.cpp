#include "client.h"

LockingServiceClient::LockingServiceClient(
    const std::shared_ptr<Channel> &channel) {
  stub_ = LockingService::NewStub(channel);
}

void LockingServiceClient::requestSharedLock(unsigned int transactionId,
                                             unsigned int rowId) {
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  spdlog::debug("making call to stub");
  Status status = stub_->LockShared(&context, request, &response);

  if (status.ok()) {
    spdlog::info("Shared lock acquired");
  } else {
    throw std::domain_error("Request failed");
  }
}

void LockingServiceClient::requestExclusiveLock(unsigned int transactionId,
                                                unsigned int rowId) {
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->LockExclusive(&context, request, &response);

  if (status.ok()) {
    spdlog::info("Exclusive lock acquired");
  } else {
    throw std::domain_error("Request failed");
  }
}

auto LockingServiceClient::requestUnlock(unsigned int transactionId,
                                         unsigned int rowId) -> bool {
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->Unlock(&context, request, &response);

  return status.ok();
}