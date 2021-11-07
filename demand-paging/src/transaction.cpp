#include "transaction.h"

Transaction* newTransaction(int transactionId, int lockBudget) {
  Transaction* transaction = new Transaction();
  transaction->transaction_id = transactionId;
  transaction->aborted = false;
  transaction->growing_phase = true;
  transaction->lock_budget = lockBudget;
  transaction->locked_rows_size = 0;
  transaction->locked_rows = nullptr;
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
    transaction->locked_rows_size++;
    int* temp = new int[transaction->locked_rows_size];
    temp[transaction->locked_rows_size - 1] = rowId;
    for (int i = 0; i < transaction->locked_rows_size - 1; i++) {
      temp[i] = transaction->locked_rows[i];
    }
    delete[] transaction->locked_rows;
    transaction->locked_rows = temp;
    transaction->lock_budget--;
  }

  return ret;
};

void releaseLock(Transaction* transaction, int rowId, HashTable* lockTable) {
  if (!hasLock(transaction, rowId)) {
    return;
  }

  int* temp = nullptr;
  if (transaction->locked_rows_size > 1) {
    temp = new int[transaction->locked_rows_size - 1];
    int temp_i = 0;
    int row;
    for (int i = 0; i < transaction->locked_rows_size; i++) {
      row = transaction->locked_rows[i];
      if (!row == rowId) {
        temp[temp_i] = row;
        temp_i++;
      }
    }
  }

  transaction->locked_rows_size--;
  delete[] transaction->locked_rows;
  transaction->locked_rows = temp;
  transaction->growing_phase = false;

  auto lock = (Lock*)get(lockTable, rowId);
  release(lock, transaction->transaction_id);

  if (lock->owners.size() == 0) {
    remove(lockTable, rowId);
    delete lock;
  }
};

auto hasLock(Transaction* transaction, int rowId) -> bool {
  for (int i = 0; i < transaction->locked_rows_size; i++) {
    if (transaction->locked_rows[i] == rowId) {
      return true;
    }
  }
  return false;
};

void releaseAllLocks(Transaction* transaction, HashTable* lockTable) {
  for (int i = 0; i < transaction->locked_rows_size; i++) {
    int locked_row = transaction->locked_rows[i];
    auto lock = (Lock*)get(lockTable, locked_row);
    release(lock, transaction->transaction_id);
    if (lock->owners.size() == 0) {
      remove(lockTable, locked_row);
    }
  }
  transaction->locked_rows_size = 0;
  delete[] transaction->locked_rows;
  transaction->locked_rows = nullptr;
  transaction->aborted = true;
};