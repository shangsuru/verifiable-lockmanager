#include "server.h"

auto LockingServiceImpl::RegisterTransaction(ServerContext* context,
                                             const RegistrationRequest* request,
                                             RegistrationResponse* response)
    -> Status {
  int transaction_id = request->transaction_id();
  int lock_budget = request->lock_budget();

  if (lockManager_.registerTransaction(transaction_id, lock_budget)) {
    return Status::OK;
  }

  return Status::CANCELLED;
};

auto LockingServiceImpl::LockExclusive(ServerContext* context,
                                       const LockRequest* request,
                                       LockResponse* response) -> Status {
  int transaction_id = request->transaction_id();
  int row_id = request->row_id();
  bool wait_for_signature = request->wait_for_signature();

  auto [signature, ok] =
      lockManager_.lock(transaction_id, row_id, true, wait_for_signature);

  response->set_signature(
      signature);  // If not ok, signature contains an error message instead
  if (ok) {
    return Status::OK;
  }
  return Status::CANCELLED;
}

auto LockingServiceImpl::LockShared(ServerContext* context,
                                    const LockRequest* request,
                                    LockResponse* response) -> Status {
  int transaction_id = request->transaction_id();
  int row_id = request->row_id();
  bool wait_for_signature = request->wait_for_signature();

  auto [signature, ok] =
      lockManager_.lock(transaction_id, row_id, false, wait_for_signature);

  response->set_signature(
      signature);  // If not ok, signature contains an error message instead
  if (ok) {
    return Status::OK;
  }
  return Status::CANCELLED;
}

auto LockingServiceImpl::Unlock(ServerContext* context,
                                const LockRequest* request,
                                LockResponse* response) -> Status {
  int transaction_id = request->transaction_id();
  int row_id = request->row_id();
  bool wait_for_signature = request->wait_for_signature();

  lockManager_.unlock(transaction_id, row_id, wait_for_signature);
  return Status::OK;
}