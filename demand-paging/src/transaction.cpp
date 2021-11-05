#include "transaction.h"

Transaction* newTransaction(int transactionId, int lockBudget) {
  Transaction* transaction = new Transaction();
  transaction->transaction_id = transactionId;
  transaction->aborted = false;
  transaction->growing_phase = true;
  transaction->lock_budget = lockBudget;
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
    transaction->locked_rows.insert(rowId);
    transaction->lock_budget--;
  }

  return ret;
};

void releaseLock(Transaction* transaction, int rowId, HashTable* lockTable) {
  if (!hasLock(transaction, rowId)) {
    return;
  }

  transaction->locked_rows.erase(rowId);
  transaction->growing_phase = false;

  auto lock = (Lock*)get(lockTable, rowId);
  release(lock, transaction->transaction_id);

  if (lock->owners.size() == 0) {
    remove(lockTable, rowId);
    delete lock;
  }
};

auto hasLock(Transaction* transaction, int rowId) -> bool {
  return transaction->locked_rows.find(rowId) != transaction->locked_rows.end();
};

void releaseAllLocks(Transaction* transaction, HashTable* lockTable) {
  for (int locked_row : transaction->locked_rows) {
    auto lock = (Lock*)get(lockTable, locked_row);
    release(lock, transaction->transaction_id);
    if (lock->owners.size() == 0) {
      remove(lockTable, locked_row);
    }
  }
  transaction->locked_rows.clear();
  transaction->aborted = true;
};