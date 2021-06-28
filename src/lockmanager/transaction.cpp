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

void Transaction::deleteLock(unsigned int rowId) {
  lockedRows_.erase(rowId);
  phase_ = Phase::kShrinking;
};