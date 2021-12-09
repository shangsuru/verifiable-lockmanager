#include "lock.h"

Lock* newLock() {
  Lock* lock = new Lock();
  lock->exclusive = false;
  lock->owners_size = 0;
  lock->owners = nullptr;
  return lock;
}

auto getSharedAccess(Lock* lock, int transactionId) -> bool {
  if (!lock->exclusive) {
    bool alreadyOwnsLock = false;
    for (int i = 0; i < lock->owners_size; i++) {
      if (lock->owners[i] == transactionId) {
        alreadyOwnsLock = true;
        break;
      }
    }

    if (!alreadyOwnsLock) {
      int* temp = new int[lock->owners_size + 1];
      temp[lock->owners_size] = transactionId;

      for (int i = 0; i < lock->owners_size; i++) {
        temp[i] = lock->owners[i];
      }

      lock->owners_size++;
      delete[] lock->owners;
      lock->owners = temp;

      return true;
    }
  }
  return false;
};

auto getExclusiveAccess(Lock* lock, int transactionId) -> bool {
  if (lock->owners_size == 0) {
    lock->exclusive = true;
    lock->owners = new int[1];
    lock->owners[0] = transactionId;
    lock->owners_size = 1;

    return true;
  }
  return false;
};

auto upgrade(Lock* lock, int transactionId) -> bool {
  if (lock->owners_size == 1) {
    for (int i = 0; i < lock->owners_size; i++) {
      if (lock->owners[i] == transactionId) {
        lock->exclusive = true;
        return true;
      }
    }
  }
  return false;
};

void release(Lock* lock, int transactionId) {
  bool transactionOwnsLock = false;
  for (int i = 0; i < lock->owners_size; i++) {
    if (lock->owners[i] == transactionId) {
      transactionOwnsLock = true;
      break;
    }
  }

  if (transactionOwnsLock) {
    if (lock->owners_size > 1) {
      for (int i = 0; i < lock->owners_size; i++) {
        if (lock->owners[i] == transactionId) {
          for (int j = i; j < lock->owners_size - 1; j++) {
            lock->owners[i] = lock->owners[i + 1];
          }
          break;
        }
      }
    }

    lock->owners_size--;
    lock->owners =
        (int*)realloc(lock->owners, sizeof(int) * (lock->owners_size));
    lock->exclusive = false;
  }
}