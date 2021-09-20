#pragma once

#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>

using std::memcpy;

const int kTransactionBudget = 200;

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