#include "lock.h"

Lock* newLock() {
  Lock* lock = new Lock();
  lock->exclusive = false;
  lock->owners = (int*)malloc(sizeof(int) * kTransactionBudget);
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
      memcpy((void*)&lock->owners[i], (void*)&lock->owners[i + 1],
             sizeof(int) * (lock->num_owners - 1 - i));
      wasOwner = true;
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

  copy->owners = (int*)malloc(sizeof(int) * num_owners);
  for (int i = 0; i < num_owners; i++) {
    copy->owners[i] = lock->owners[i];
  }

  return (void*)copy;
}