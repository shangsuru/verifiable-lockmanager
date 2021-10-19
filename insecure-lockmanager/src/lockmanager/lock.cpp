#include "lock.h"

Lock* newLock() {
  Lock* lock = new Lock();
  lock->exclusive = false;
  return lock;
}

auto getSharedAccess(Lock* lock, int transactionId) -> bool {
  if (!lock->exclusive) {
    lock->owners.insert(transactionId);
    return true;
  }
  return false;
};

auto getExclusiveAccess(Lock* lock, int transactionId) -> bool {
  if (lock->owners.size() == 0) {
    lock->exclusive = true;
    lock->owners.insert(transactionId);
    return true;
  }
  return false;
};

auto upgrade(Lock* lock, int transactionId) -> bool {
  if (lock->owners.size() == 1 &&
      lock->owners.find(transactionId) != lock->owners.end()) {
    lock->exclusive = true;
    return true;
  }
  return false;
};

void release(Lock* lock, int transactionId) {
  if (lock->owners.find(transactionId) != lock->owners.end()) {
    lock->owners.erase(transactionId);
    lock->exclusive = false;
  }
}