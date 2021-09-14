#include "lock.h"

auto Lock::getMode() -> LockMode {
  if (exclusive_) {
    return LockMode::kExclusive;
  }
  return LockMode::kShared;
}

auto Lock::getSharedAccess(unsigned int transactionId) -> int {
  if (!exclusive_) {
    owners_.insert(transactionId);
  } else {
    print_error("Couldn't acquire shared lock");
    return SGX_ERROR_UNEXPECTED;
  }

  return SGX_SUCCESS;
};

auto Lock::getExclusiveAccess(unsigned int transactionId) -> int {
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
  if (owners_.size() == 1 && (owners_.count(transactionId) == 1)) {
    exclusive_ = true;
  } else {
    print_error("Couldn't upgrade lock");
    return SGX_ERROR_UNEXPECTED;
  }

  return SGX_SUCCESS;
};

void Lock::release(unsigned int transactionId) {
  exclusive_ = false;
  owners_.erase(transactionId);
}

auto Lock::getOwners() -> std::set<unsigned int> { return owners_; }
