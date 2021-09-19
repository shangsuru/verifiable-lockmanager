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

auto addLock(Transaction* transaction, int rowId, Lock::LockMode requestedMode,
             Lock* lock) -> bool {
  if (transaction->aborted) {
    return false;
  }

  bool ret;
  switch (requestedMode) {
    case Lock::LockMode::kExclusive:
      ret = lock->getExclusiveAccess(transaction->transaction_id);
      break;
    case Lock::LockMode::kShared:
      ret = lock->getSharedAccess(transaction->transaction_id);
      break;
  }

  if (ret) {
    // lockbudget (max) to copy everything
    transaction->locked_rows[transaction->num_locked++] = rowId;
    transaction->lock_budget--;
  }

  return ret;
};

void releaseLock(Transaction* transaction, int rowId,
                 std::unordered_map<int, Lock*>* lockTable) {
  for (int i = 0; i < transaction->num_locked; i++) {
    if (transaction->locked_rows[i] == rowId) {
      memcpy((void*)transaction->locked_rows[i],
             (void*)transaction->locked_rows[i + 1],
             sizeof(int) * (transaction->num_locked - 1 - i));
    }
    break;
  }
  transaction->growing_phase = false;
  auto lock = (*lockTable)[rowId];
  lock->release(transaction->transaction_id);

  if (lock->getOwners().size() == 0) {
    delete lock;
    lockTable->erase(rowId);
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

void releaseAllLocks(Transaction* transaction,
                     std::unordered_map<int, Lock*>* lockTable) {
  for (int i = 0; i < transaction->num_locked; i++) {
    int locked_row = transaction->locked_rows[i];
    auto lock = (*lockTable)[locked_row];
    lock->release(transaction->transaction_id);
    if (lock->getOwners().size() == 0) {
      delete lock;
      lockTable->erase(locked_row);
    }
  }
  transaction->num_locked = 0;
  transaction->aborted = true;
};