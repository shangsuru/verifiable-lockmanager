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
   * @throws std::domain_error, when the lock is exclusive
   */
  void getSharedAccess();

  /**
   * Attempts to acquire exclusive access for a transaction.
   *
   * @throws std::domain_error, if someone already has that lock
   */
  void getExclusiveAccess();

  /**
   * Releases the lock for the calling transaction.
   */
  void release();

  /**
   * Upgrades the lock for the transaction, that currently holds the shared lock
   * alone, to an exclusive lock.
   *
   * @throws std::domain_error, if some other transaction currently still has
   * shared access to the lock
   */
  void upgrade();

 private:
  bool exclusive_;
  unsigned int numSharedOwners_;
  std::mutex mut_;
};