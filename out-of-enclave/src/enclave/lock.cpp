#include "lock.h"

auto Lock::getMode() -> LockMode {
  const std::lock_guard<std::mutex> latch(mut_);

  if (exclusive_) {
    return LockMode::kExclusive;
  }
  return LockMode::kShared;
}

auto Lock::getSharedAccess(unsigned int transactionId) -> int {
  const std::lock_guard<std::mutex> latch(mut_);

  if (!exclusive_) {
    owners_.insert(transactionId);
  } else {
    print_error("Couldn't acquire shared lock");
    return SGX_ERROR_UNEXPECTED;
  }

  return SGX_SUCCESS;
};

auto Lock::getExclusiveAccess(unsigned int transactionId) -> int {
  const std::lock_guard<std::mutex> latch(mut_);

  if (owners_.empty()) {
    exclusive_ = true;
    owners_.insert(transactionId);
  } else {
    print_error("Couldn't acquire exclusive lock");
    return SGX_ERROR_UNEXPECTED;
  }

  return SGX_SUCCESS;
};

auto Lock::upgrade(unsigned int transactionId) -> int {
  const std::lock_guard<std::mutex> latch(mut_);

  if (owners_.size() == 1 && (owners_.count(transactionId) == 1)) {
    exclusive_ = true;
  } else {
    print_error("Couldn't upgrade lock");
    return SGX_ERROR_UNEXPECTED;
  }

  return SGX_SUCCESS;
};

void Lock::release(unsigned int transactionId) {
  const std::lock_guard<std::mutex> latch(mut_);

  exclusive_ = false;
  owners_.erase(transactionId);
}

auto Lock::getOwners() -> std::set<unsigned int> { return owners_; }
