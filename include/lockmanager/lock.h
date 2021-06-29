#pragma once

#include <mutex>
#include <set>
#include <stdexcept>

/**
 * The internal representation of a lock for the lock manager.
 */
class Lock {
 public:
  /**
   * A lock can either be shared, then it allows multiple reads
   * or exclusive, then it allows only one write access
   */
  enum LockMode { kShared, kExclusive };

  /**
   * @returns the mode (shared, exclusive) the lock is in
   */
  auto getMode() -> LockMode;

  /**
   * Attempts to acquire shared access for a transaction.
   *
   * @param transactionId ID of the transaction, that wants to acquire the lock
   * @throws std::domain_error, when the lock is exclusive
   */
  void getSharedAccess(unsigned int transactionId);

  /**
   * Attempts to acquire exclusive access for a transaction.
   *
   * @param transactionId ID of the transaction, that wants to acquire the lock
   * @throws std::domain_error, if someone already has that lock
   */
  void getExclusiveAccess(unsigned int transactionId);

  /**
   * Releases the lock for the calling transaction.
   *
   * @param transactionId ID of the calling transaction
   */
  void release(unsigned int transactionId);

  /**
   * Upgrades the lock for the transaction, that currently holds the shared lock
   * alone, to an exclusive lock.
   *
   * @param transactionId ID of the transaction, that wants to acquire the lock
   * @throws std::domain_error, if some other transaction currently still has
   * shared access to the lock
   */
  void upgrade(unsigned int transactionId);

  /**
   * @returns the set of transaction IDs that hold this lock
   */
  auto getOwners() -> std::set<unsigned int>;

 private:
  bool exclusive_;
  std::set<unsigned int>
      owners_;  // IDs of the transactions that currently hold this lock
  std::mutex mut_;
};