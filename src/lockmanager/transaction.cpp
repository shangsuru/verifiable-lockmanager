#include "transaction.h"

Transaction::Transaction(unsigned int transactionId, unsigned int lockBudget)
    : transactionId_(transactionId), lockBudget_(lockBudget){};

auto Transaction::getLockedRows() const -> std::set<unsigned int> {
  return lockedRows_;
};

auto Transaction::getLockBudget() const -> unsigned int { return lockBudget_; };

auto Transaction::getPhase() -> Phase { return phase_; };

void Transaction::addLock(unsigned int rowId, Lock::LockMode requestedMode,
                          std::shared_ptr<Lock>& lock) {
  if (aborted_) {
    return;
  }
  std::shared_lock latch(mut_);

  switch (requestedMode) {
    case Lock::LockMode::kExclusive:
      lock->getExclusiveAccess(transactionId_);
      break;
    case Lock::LockMode::kShared:
      lock->getSharedAccess(transactionId_);
      break;
  }

  lockedRows_.insert(rowId);
  lockBudget_--;
};

void Transaction::releaseLock(unsigned int rowId, std::shared_ptr<Lock>& lock) {
  if (this->hasLock(rowId)) {
    phase_ = Phase::kShrinking;
    lockedRows_.erase(rowId);
    lock->release(transactionId_);
  }
};

auto Transaction::hasLock(unsigned int rowId) -> bool {
  return lockedRows_.find(rowId) != lockedRows_.end();
};

void Transaction::releaseAllLocks(
    cuckoohash_map<unsigned int, std::shared_ptr<Lock>>& lockTable) {
  std::unique_lock latch(mut_);

  for (auto locked_row : lockedRows_) {
    lockTable.find(locked_row)->release(transactionId_);
  }
  lockedRows_.clear();
  aborted_ = true;
};

auto Transaction::getTransactionId() const -> unsigned int {
  return transactionId_;
}