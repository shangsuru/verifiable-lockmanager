#include "lock.h"

auto Lock::getMode() -> LockMode {
  const std::lock_guard<std::mutex> latch(mut_);

  if (exclusive_) {
    return LockMode::kExclusive;
  }
  return LockMode::kShared;
}

void Lock::getSharedAccess() {
  const std::lock_guard<std::mutex> latch(mut_);

  if (!exclusive_) {
    numSharedOwners_++;
  } else {
    throw std::domain_error("Couldn't acquire lock");
  }
};

void Lock::getExclusiveAccess() {
  const std::lock_guard<std::mutex> latch(mut_);

  if (!exclusive_ && numSharedOwners_ == 0) {
    exclusive_ = true;
  } else {
    throw std::domain_error("Couldn't acquire lock");
  }
};

void Lock::upgrade() {
  const std::lock_guard<std::mutex> latch(mut_);

  if (numSharedOwners_ == 1) {
    numSharedOwners_ = 0;
    exclusive_ = true;
  } else {
    throw std::domain_error("Couldn't acquire lock");
  }
};

void Lock::release() {
  const std::lock_guard<std::mutex> latch(mut_);

  if (exclusive_) {
    exclusive_ = false;
  } else {
    numSharedOwners_--;
  }

  // TODO delete the lock from the lock table if it is shared with no owners
}
