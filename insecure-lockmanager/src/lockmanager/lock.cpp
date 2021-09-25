#include "lock.h"

auto Lock::getMode() -> LockMode {
  if (exclusive_) {
    return LockMode::kExclusive;
  }
  return LockMode::kShared;
}

auto Lock::getSharedAccess(unsigned int transactionId) -> bool {
  if (!exclusive_) {
    owners_.insert(transactionId);
  } else {
    spdlog::error("Couldn't acquire shared lock");
    return false;
  }

  return true;
};

auto Lock::getExclusiveAccess(unsigned int transactionId) -> bool {
  if (owners_.empty()) {
    exclusive_ = true;
    owners_.insert(transactionId);
  } else {
    spdlog::error("Couldn't acquire exclusive lock");
    return false;
  }

  return true;
};

auto Lock::upgrade(unsigned int transactionId) -> bool {
  if (owners_.size() == 1 && (owners_.count(transactionId) == 1)) {
    exclusive_ = true;
  } else {
    spdlog::error("Couldn't upgrade lock");
    return false;
  }

  return true;
};

void Lock::release(unsigned int transactionId) {
  if (owners_.find(transactionId) != owners_.end()) {
    exclusive_ = false;
    owners_.erase(transactionId);
  }
}

auto Lock::getOwners() -> std::set<unsigned int> { return owners_; }
