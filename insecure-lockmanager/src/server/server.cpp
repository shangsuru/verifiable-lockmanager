#include "server.h"

auto LockingServiceImpl::RegisterTransaction(ServerContext* context,
                                             const RegistrationRequest* request,
                                             RegistrationResponse* response)
    -> Status {
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
  bool wait_for_signature = request->wait_for_signature();

  if (lockManager_.lock(transaction_id, row_id, true, wait_for_signature)) {
    return Status::OK;
  }
  return Status::CANCELLED;
}

auto LockingServiceImpl::LockShared(ServerContext* context,
                                    const LockRequest* request,
                                    LockResponse* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();
  bool wait_for_signature = request->wait_for_signature();

  if (lockManager_.lock(transaction_id, row_id, false, wait_for_signature)) {
    return Status::OK;
  }
  return Status::CANCELLED;
}

auto LockingServiceImpl::Unlock(ServerContext* context,
                                const LockRequest* request,
                                LockResponse* response) -> Status {
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();
  bool wait_for_signature = request->wait_for_signature();

  lockManager_.unlock(transaction_id, row_id, wait_for_signature);
  return Status::OK;
}