#include "server.h"

auto LockingServiceImpl::RegisterTransaction(ServerContext* context,
                                             const Registration* request,
                                             Acceptance* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int lock_budget = request->lock_budget();

  if (lockManager_.registerTransaction(transaction_id)) {
    return Status::OK;
  }

  return Status::CANCELLED;
};

auto LockingServiceImpl::LockExclusive(ServerContext* context,
                                       const LockRequest* request,
                                       LockResponse* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  auto [signature, ok] = lockManager_.lock(transaction_id, row_id, true);

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
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  auto [signature, ok] = lockManager_.lock(transaction_id, row_id, false);

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
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  lockManager_.unlock(transaction_id, row_id);
  return Status::OK;
}