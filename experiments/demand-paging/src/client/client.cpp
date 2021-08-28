#include "client.h"

LockingServiceClient::LockingServiceClient(
    const std::shared_ptr<Channel> &channel) {
  stub_ = LockingService::NewStub(channel);
}

auto LockingServiceClient::registerTransaction(unsigned int transactionId,
                                               unsigned int lockBudget)
    -> bool {
  Registration registration;
  registration.set_transaction_id(transactionId);
  registration.set_lock_budget(lockBudget);

  Acceptance acceptance;
  ClientContext context;

  Status status =
      stub_->RegisterTransaction(&context, registration, &acceptance);

  return status.ok();
};

auto LockingServiceClient::requestSharedLock(unsigned int transactionId,
                                             unsigned int rowId)
    -> std::string {
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->LockShared(&context, request, &response);

  if (status.ok()) {
    return response.signature();
  }

  throw std::domain_error("Request failed");
}

auto LockingServiceClient::requestExclusiveLock(unsigned int transactionId,
                                                unsigned int rowId)
    -> std::string {
  LockRequest request;
  request.set_transaction_id(transactionId);
  request.set_row_id(rowId);

  LockResponse response;
  ClientContext context;

  Status status = stub_->LockExclusive(&context, request, &response);

  if (status.ok()) {
    return response.signature();
  }

  throw std::domain_error("Request failed");
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