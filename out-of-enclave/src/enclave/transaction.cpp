#include "transaction.h"

Transaction::Transaction(unsigned int transactionId, unsigned int lockBudget)
    : transactionId_(transactionId), lockBudget_(lockBudget){};

auto Transaction::getLockedRows() -> std::set<unsigned int> {
  const std::lock_guard<std::mutex> latch(mut_);

  return lockedRows_;
};

auto Transaction::getLockBudget() const -> unsigned int { return lockBudget_; };

auto Transaction::getPhase() -> Phase { return phase_; };

void Transaction::addLock(unsigned int rowId, Lock::LockMode requestedMode,
                          Lock* lock) {
  const std::lock_guard<std::mutex> latch(mut_);

  if (aborted_) {
    return;
  }

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

void Transaction::releaseLock(unsigned int rowId, Lock* lock) {
  const std::lock_guard<std::mutex> latch(mut_);

  if (lockedRows_.find(rowId) != lockedRows_.end()) {
    phase_ = Phase::kShrinking;
    lockedRows_.erase(rowId);
    lock->release(transactionId_);
  }
};

auto Transaction::hasLock(unsigned int rowId) -> bool {
  const std::lock_guard<std::mutex> latch(mut_);

  return lockedRows_.find(rowId) != lockedRows_.end();
};

void Transaction::releaseAllLocks(
    std::unordered_map<unsigned int, Lock*>& lockTable) {
  const std::lock_guard<std::mutex> latch(mut_);

  for (auto locked_row : lockedRows_) {
    lockTable[locked_row]->release(transactionId_);
  }
  lockedRows_.clear();
  aborted_ = true;
};

auto Transaction::getTransactionId() const -> unsigned int {
  return transactionId_;
}