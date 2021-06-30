#include "server.h"

auto LockingServiceImpl::RegisterTransaction(ServerContext* context,
                                             const Registration* request,
                                             Acceptance* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int lock_budget = request->lock_budget();

  try {
    lockManager_.registerTransaction(transaction_id, lock_budget);
  } catch (const std::domain_error& e) {
    return Status::CANCELLED;
  }

  return Status::OK;
};

auto LockingServiceImpl::LockExclusive(ServerContext* context,
                                       const LockRequest* request,
                                       LockResponse* response) -> Status {
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

auto LockingServiceImpl::LockShared(ServerContext* context,
                                    const LockRequest* request,
                                    LockResponse* response) -> Status {
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

auto LockingServiceImpl::Unlock(ServerContext* context,
                                const LockRequest* request,
                                LockResponse* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  try {
    lockManager_.unlock(transaction_id, row_id);
  } catch (const std::domain_error& e) {
    return Status::CANCELLED;
  }

  return Status::OK;
}