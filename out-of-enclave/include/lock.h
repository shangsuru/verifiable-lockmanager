#pragma once

#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>

using std::memcpy;

/**
 * Used to set the size of the owners list of a lock. As of right now, there is
 * no way implemented to dynamically resize untrusted memory from the trusted
 * region. Therefore we allocate as much memory as we could possibly need. So we
 * have to set an upper limit on the number of owners of a lock, i.e. the number
 * of concurrent transactions.
 */
const int kTransactionBudget = 2;

/**
 * The internal representation of a lock for the lock manager.
 */
struct Lock {
  bool exclusive;
  int* owners;
  int num_owners;
};
typedef struct Lock Lock;

/**
 * Initializes a lock struct
 */
Lock* newLock();

/**
 * Attempts to acquire shared access for a transaction.
 *
 * @param lock the lock the operation is executed on
 * @param transactionId ID of the transaction, that wants to acquire the lock
 * @throws std::domain_error, when the lock is exclusive
 */
auto getSharedAccess(Lock* lock, int transactionId) -> bool;

/**
 * Attempts to acquire exclusive access for a transaction.
 *
 * @param lock the lock the operation is executed on
 * @param transactionId ID of the transaction, that wants to acquire the lock
 * @throws std::domain_error, if someone already has that lock
 */
auto getExclusiveAccess(Lock* lock, int transactionId) -> bool;

/**
 * Releases the lock for the calling transaction.
 * @param lock the lock the operation is executed on
 * @param transactionId ID of the calling transaction
 */
void release(Lock* lock, int transactionId);

/**
 * Upgrades the lock for the transaction, that currently holds the shared lock
 * alone, to an exclusive lock.
 *
 * @param lock the lock the operation is executed on
 * @param transactionId ID of the transaction, that wants to acquire the lock
 * @throws std::domain_error, if some other transaction currently still has
 * shared access to the lock
 */
auto upgrade(Lock* lock, int transactionId) -> bool;

/**
 * Creates a new lock that has the same content as the given lock. This
 * is used to move a lock that is allocated in untrusted memory into protected
 * memory.
 *
 * @param lock the lock to copy
 * @return copy casted as void*
 */
auto copy_lock(Lock* lock) -> void*;

/**
 * Frees the memory allocated when calling copy_lock()
 *
 * @param lock the lock created with copy_lock()
 */
void free_lock_copy(Lock*& lock);