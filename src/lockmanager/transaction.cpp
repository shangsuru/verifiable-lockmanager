#include "transaction.h"

auto Transaction::getLockedRows() const -> std::set<unsigned int> {
  return lockedRows_;
};

auto Transaction::getLockBudget() const -> unsigned int { return lockBudget_; };

auto Transaction::getPhase() -> Phase { return phase_; };

void Transaction::addLock(unsigned int rowId) {
  lockedRows_.insert(rowId);
  lockBudget_--;
};

void Transaction::releaseLock(unsigned int rowId, std::shared_ptr<Lock>& lock) {
  const std::lock_guard<std::mutex> latch(mut_);

  if (this->hasLock(rowId)) {
    phase_ = Phase::kShrinking;
    lockedRows_.erase(rowId);
    lock->release();
  }
};

auto Transaction::hasLock(unsigned int rowId) -> bool {
  return lockedRows_.find(rowId) != lockedRows_.end();
};