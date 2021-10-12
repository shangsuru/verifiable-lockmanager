#include "transaction.h"

Transaction* newTransaction(int transactionId, int lockBudget) {
  Transaction* transaction = new Transaction();
  transaction->transaction_id = transactionId;
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
    transaction->locked_rows[transaction->num_locked++] = rowId;
    transaction->lock_budget--;
  }

  return ret;
};

void releaseLock(Transaction* transaction, int rowId, HashTable* lockTable) {
  bool wasOwner = false;
  for (int i = 0; i < transaction->num_locked; i++) {
    if (transaction->locked_rows[i] == rowId) {
      memcpy((void*)&transaction->locked_rows[i],
             (void*)&transaction->locked_rows[i + 1],
             sizeof(int) * (transaction->num_locked - 1 - i));
      wasOwner = true;
    }
    break;
  }

  if (!wasOwner) {
    return;
  }

  transaction->num_locked--;
  transaction->growing_phase = false;
  auto lock = (Lock*)get(lockTable, rowId);
  release(lock, transaction->transaction_id);

  if (lock->num_owners == 0) {
    remove(lockTable, rowId);
  }
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

  copy->locked_rows = new int[num_locked];
  for (int i = 0; i < num_locked; i++) {
    copy->locked_rows[i] = transaction->locked_rows[i];
  }

  return (void*)copy;
}

void free_transaction_copy(Transaction*& transaction) {
  free(transaction->locked_rows);
  delete transaction;
}