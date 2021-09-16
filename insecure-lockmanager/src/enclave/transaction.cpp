#include "transaction.h"

Transaction::Transaction(unsigned int transactionId)
    : transactionId_(transactionId){};

auto Transaction::getLockedRows() -> std::set<unsigned int> {
  return lockedRows_;
};

auto Transaction::getPhase() -> Phase { return phase_; };

auto Transaction::addLock(unsigned int rowId, Lock::LockMode requestedMode,
                          Lock* lock) -> int {
  if (aborted_) {
    return SGX_ERROR_UNEXPECTED;
  }

  int ret;
  switch (requestedMode) {
    case Lock::LockMode::kExclusive:
      ret = lock->getExclusiveAccess(transactionId_);
      break;
    case Lock::LockMode::kShared:
      ret = lock->getSharedAccess(transactionId_);
      break;
  }

  if (ret == SGX_SUCCESS) {
    lockedRows_.insert(rowId);
  }

  return ret;
};

void Transaction::releaseLock(
    unsigned int rowId, std::unordered_map<unsigned int, Lock*>& lockTable) {
  if (lockedRows_.find(rowId) != lockedRows_.end()) {
    phase_ = Phase::kShrinking;
    lockedRows_.erase(rowId);
    auto lock = lockTable[rowId];
    lock->release(transactionId_);
    if (lock->getOwners().size() == 0) {
      delete lock;
      lockTable.erase(rowId);
    }
  }
};

auto Transaction::hasLock(unsigned int rowId) -> bool {
  return lockedRows_.find(rowId) != lockedRows_.end();
};

void Transaction::releaseAllLocks(
    std::unordered_map<unsigned int, Lock*>& lockTable) {
  for (auto locked_row : lockedRows_) {
    auto lock = lockTable[locked_row];
    lock->release(transactionId_);
    if (lock->getOwners().size() == 0) {
      delete lock;
      lockTable.erase(locked_row);
    }
  }
  lockedRows_.clear();
  aborted_ = true;
};

auto Transaction::getTransactionId() const -> unsigned int {
  return transactionId_;
}