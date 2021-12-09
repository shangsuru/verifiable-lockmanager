#pragma once

#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include "hashtable.h"
#include "lock.h"

using std::memcpy;

/**
 * The internal representation of a transaction for the lock manager.
 * It keeps track of the lock budget, i.e. the maximum number of locks
 * the transaction is allowed to acquire, the set of acquired locks of
 * that transaction as well as its phase according to 2PL.
 */
struct Transaction {
  int transaction_id;
  bool aborted;
  /**
   * According to 2PL, a transaction has two subsequent phases:
   * It starts with the growing phase, where it acquires all
   * the necessary locks at once. After the first lock is released,
   * it enters the shrinking phase. From thereon, the transaction is
   * not allowed to acquire new locks and only continues to release
   * the already existent locks.
   */
  bool growing_phase;
  int lock_budget;
  int* locked_rows;
  int locked_rows_size;
  int num_locked;
};
typedef struct Transaction Transaction;

/**
 * Initializes the transaction struct. New transaction objects, that are created
 * for new, not-yet registered transaction beforehand by the untrusted
 * application always have their transaction ID set to -1 to differentiate them
 * from transaction objects refering to already registered transactions.
 *
 * @param transactionId identifies the transaction
 * @param lockBudget maximum number of locks the transaction is allowed to
 * acquire
 * @returns a pointer to the transaction struct
 */
Transaction* newTransaction(int transactionId, int lockBudget);

/**
 * When the transaction acquires a new lock, the row ID that lock refers to is
 * added to the set of locked rows and it decrements the lock budget by 1.
 * Then it tries to acquire the requested mode (shared or exclusive) for the
 * given lock.
 *
 * @param Transaction transaction to execute the operation on
 * @param rowId row ID of the newly acquired lock
 * @param requestedMode
 * @param lock
 */
auto addLock(Transaction* transaction, int rowId, bool isExclusive, Lock* lock)
    -> bool;

/**
 * Checks if the transaction currently holds a lock on the given row ID.
 * If so, it enters the shrinking phase and removes the row ID from the set of
 * locked rows. Then it releases the lock.
 *
 * @param Transaction transaction to execute the operation on
 * @param rowId row ID of the released lock
 * @param lock the lock to release
 */
void releaseLock(Transaction* transaction, int rowId, HashTable* lockTable);

/**
 * Checks if the transaction has a lock on the specified row.
 *
 * @param Transaction transaction to execute the operation on
 * @param rowId
 * @returns true if it has lock, else false
 */
auto hasLock(Transaction* transaction, int rowId) -> bool;

/**
 * Releases all the locks, that the transaction holds. This is supposed to be
 * called when the transaction aborts.
 *
 * @param Transaction transaction to execute the operation on
 * @param lockTable containing all the locks indexed by row ID
 */
void releaseAllLocks(Transaction* transaction, HashTable* lockTable);

/**
 * Creates a new transaction that has the same content as the given transaction.
 * This is used to move a transaction that is allocated in untrusted memory into
 * protected memory.
 *
 * @param transaction the transaction to copy
 * @return copy casted as void*
 */
auto copy_transaction(Transaction* transaction) -> void*;

/**
 * Frees the memory allocated when calling copy_transaction()
 *
 * @param transaction the transaction created with copy_transaction())
 */
void free_transaction_copy(Transaction*& transaction);
