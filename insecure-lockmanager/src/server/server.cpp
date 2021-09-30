#include "server.h"

auto LockingServiceImpl::RegisterTransaction(ServerContext* context,
                                             const RegistrationRequest* request,
                                             RegistrationResponse* response) -> Status {
  unsigned int transaction_id = request->transaction_id();

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

  if (lockManager_.lock(transaction_id, row_id, true)) {
    return Status::OK;
  }
  return Status::CANCELLED;
}

auto LockingServiceImpl::LockShared(ServerContext* context,
                                    const LockRequest* request,
                                    LockResponse* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  if (lockManager_.lock(transaction_id, row_id, false)) {
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