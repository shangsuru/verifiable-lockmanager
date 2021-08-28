#include "server.h"

auto LockingServiceImpl::LockExclusive(ServerContext* context,
                                       const LockRequest* request,
                                       LockResponse* response) -> Status {
  spdlog::info("Getting request for exclusive lock");
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  try {
    lockManager_.lock(transaction_id, row_id, Lock::LockMode::kExclusive);
  } catch (const std::domain_error& e) {
    spdlog::warn(e.what());
    return Status::CANCELLED;
  }

  return Status::OK;
}

auto LockingServiceImpl::LockShared(ServerContext* context,
                                    const LockRequest* request,
                                    LockResponse* response) -> Status {
  spdlog::info("Getting request for shared lock");
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();
  try {
    lockManager_.lock(transaction_id, row_id, Lock::LockMode::kShared);
  } catch (const std::domain_error& e) {
    spdlog::warn(e.what());
    return Status::CANCELLED;
  }

  return Status::OK;
}

auto LockingServiceImpl::Unlock(ServerContext* context,
                                const LockRequest* request,
                                LockResponse* response) -> Status {
  spdlog::info("Getting request to unlock");
  unsigned int transaction_id = request->transaction_id();
  unsigned int row_id = request->row_id();

  try {
    lockManager_.unlock(transaction_id, row_id);
  } catch (const std::domain_error& e) {
    spdlog::warn(e.what());
    return Status::CANCELLED;
  }

  return Status::OK;
}