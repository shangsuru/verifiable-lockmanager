#include "lock.h"

auto Lock::getMode() -> LockMode {
  const std::lock_guard<std::mutex> latch(mut_);

  if (exclusive_) {
    return LockMode::kExclusive;
  }
  return LockMode::kShared;
}

void Lock::getSharedAccess(unsigned int transactionId) {
  const std::lock_guard<std::mutex> latch(mut_);

  if (!exclusive_) {
    owners_.insert(transactionId);
  } else {
    throw std::domain_error("Couldn't acquire lock");
  }
};

void Lock::getExclusiveAccess(unsigned int transactionId) {
  const std::lock_guard<std::mutex> latch(mut_);

  if (owners_.empty()) {
    exclusive_ = true;
    owners_.insert(transactionId);
  } else {
    throw std::domain_error("Couldn't acquire lock");
  }
};

void Lock::upgrade(unsigned int transactionId) {
  const std::lock_guard<std::mutex> latch(mut_);

  if (owners_.size() == 1 && (owners_.count(transactionId) == 1)) {
    exclusive_ = true;
  } else {
    throw std::domain_error("Couldn't acquire lock");
  }
};

void Lock::release(unsigned int transactionId) {
  const std::lock_guard<std::mutex> latch(mut_);

  exclusive_ = false;
  owners_.erase(transactionId);
}
