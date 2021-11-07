#include "lock.h"

Lock* newLock() {
  Lock* lock = new Lock();
  lock->exclusive = false;
  lock->owners = new int[kTransactionBudget];
  lock->num_owners = 0;
  return lock;
}

auto getSharedAccess(Lock* lock, int transactionId) -> bool {
  if (!lock->exclusive) {
    lock->owners[lock->num_owners++] = transactionId;
    return true;
  }
  return false;
};

auto getExclusiveAccess(Lock* lock, int transactionId) -> bool {
  if (lock->num_owners == 0) {
    lock->exclusive = true;
    lock->owners[lock->num_owners++] = transactionId;
    return true;
  }
  return false;
};

auto upgrade(Lock* lock, int transactionId) -> bool {
  if (lock->num_owners == 1 && lock->owners[0] == transactionId) {
    lock->exclusive = true;
    return true;
  }
  return false;
};

void release(Lock* lock, int transactionId) {
  bool wasOwner = false;  // only release when the transactionId was actually
                          // within the set of owners of that lock
  for (int i = 0; i < lock->num_owners; i++) {
    if (lock->owners[i] == transactionId) {
      for (int j = i; j < lock->num_owners - 1; j++) {
        lock->owners[j] = lock->owners[j + 1];
      }
      wasOwner = true;
      break;
    }
  }
  if (wasOwner) {
    lock->exclusive = false;
    lock->num_owners--;
  }
}

auto copy_lock(Lock* lock) -> void* {
  Lock* copy = new Lock();
  copy->exclusive = lock->exclusive;
  int num_owners = lock->num_owners;
  copy->num_owners = num_owners;

  copy->owners = new int[kTransactionBudget];
  for (int i = 0; i < num_owners; i++) {
    copy->owners[i] = lock->owners[i];
  }

  return (void*)copy;
}

void free_lock_copy(Lock*& lock) {
  delete[] lock->owners;
  delete lock;
}