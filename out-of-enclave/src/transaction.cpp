#include "transaction.h"

Transaction* newTransaction(int lockBudget) {
  Transaction* transaction = new Transaction();
  transaction->transaction_id =
      0;  // zero for newly initialized transaction structs!
  transaction->aborted = false;
  transaction->growing_phase = true;
  transaction->lock_budget = lockBudget;
  transaction->locked_rows = new int[lockBudget];
  transaction->locked_rows_size = lockBudget;
  transaction->num_locked = 0;
  return transaction;
}

auto addLock(Transaction* transaction, int rowId, bool isExclusive, Lock* lock)
    -> bool {
  if (transaction->aborted) {
    return false;
  }

  bool ret;
  if (isExclusive) {
    ret = getExclusiveAccess(lock, transaction->transaction_id);
  } else {
    ret = getSharedAccess(lock, transaction->transaction_id);
  }

  if (ret) {
    if (transaction->num_locked == 0) {
      transaction->locked_rows = new int[1];
      transaction->locked_rows[0] = rowId;
    } else {
      transaction->locked_rows =
          (int*)realloc(transaction->locked_rows,
                        sizeof(int) * (transaction->num_locked + 1));
      transaction->locked_rows[transaction->num_locked] = rowId;
    }

    transaction->num_locked++;
    transaction->lock_budget--;
  }

  return ret;
};

void releaseLock(Transaction* transaction, int rowId, HashTable* lockTable) {
  bool wasOwner = false;
  for (int i = 0; i < transaction->num_locked; i++) {
    if (transaction->locked_rows[i] == rowId) {
      for (int j = i; j < transaction->num_locked - 1; j++) {
        transaction->locked_rows[j] = transaction->locked_rows[j + 1];
      }
      wasOwner = true;
      break;
    }
  }

  if (!wasOwner) {
    return;
  }

  transaction->num_locked--;
  transaction->growing_phase = false;
  auto lock = (Lock*)get(lockTable, rowId);
  if (lock != nullptr) {
    release(lock, transaction->transaction_id);
    if (lock->num_owners == 0) {
      remove(lockTable, rowId);
      // delete lock; -> issues with deleting locks in untrusted memory
    }
  }
  return;
};

auto hasLock(Transaction* transaction, int rowId) -> bool {
  for (int i = 0; i < transaction->num_locked; i++) {
    if (transaction->locked_rows[i] == rowId) {
      return true;
    }
  }
  return false;
};

void releaseAllLocks(Transaction* transaction, HashTable* lockTable) {
  for (int i = 0; i < transaction->num_locked; i++) {
    int locked_row = transaction->locked_rows[i];
    auto lock = (Lock*)get(lockTable, locked_row);
    release(lock, transaction->transaction_id);
    if (lock->num_owners == 0) {
      remove(lockTable, locked_row);
    }
  }
  transaction->num_locked = 0;
  transaction->aborted = true;
};

auto copy_transaction(Transaction* transaction) -> void* {
  Transaction* copy = new Transaction();
  copy->transaction_id = transaction->transaction_id;
  copy->aborted = transaction->aborted;
  copy->growing_phase = transaction->growing_phase;
  copy->lock_budget = transaction->lock_budget;
  copy->locked_rows_size = transaction->locked_rows_size;

  int num_locked = transaction->num_locked;
  copy->num_locked = num_locked;

  if (copy->num_locked > 0) {
    copy->locked_rows = new int[copy->num_locked];
  } else {
    copy->locked_rows = nullptr;
  }
  for (int i = 0; i < num_locked; i++) {
    copy->locked_rows[i] = transaction->locked_rows[i];
  }

  return (void*)copy;
}

void free_transaction_copy(Transaction*& transaction) {
  delete[] transaction->locked_rows;
  delete transaction;
}