#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include "lock.h"

/**
 * The internal representation of a transaction for the lock manager.
 * It keeps track of the set of acquired locks of
 * that transaction as well as its phase according to 2PL.
 */
class Transaction {
 public:
  /**
   * @param transactionId identifying the transaction
   */
  Transaction(unsigned int transactionId);

  /**
   * According to 2PL, a transaction has two subsequent phases:
   * It starts with the growing phase, where it acquires all
   * the necessary locks at once. After the first lock is released,
   * it enters the shrinking phase. From thereon, the transaction is
   * not allowed to acquire new locks and only continues to release
   * the already existent locks.
   */
  enum Phase { kGrowing, kShrinking };

  /**
   * @returns ID uniquely identifying this transaction
   */
  [[nodiscard]] auto getTransactionId() const -> unsigned int;

  /**
   * @returns the set of row IDs the transaction curently holds locks for
   */
  [[nodiscard]] auto getLockedRows() -> std::set<unsigned int>;

  /**
   * @returns the phase the transaction is currently in, which is important for
   *          the lock manager to determine, if the transaction can acquire more
   *          locks (growing phase) or not (shrinking phase)
   */
  auto getPhase() -> Phase;

  /**
   * When the transaction acquires a new lock, the row ID that lock refers to is
   * added to the set of locked rows.
   * Then it tries to acquire the requested mode (shared or exclusive) for the
   * given lock.
   *
   * @param rowId row ID of the newly acquired lock
   * @param requestedMode
   * @param lock
   */
  auto addLock(unsigned int rowId, Lock::LockMode requestedMode, Lock* lock)
      -> int;

  /**
   * Checks if the transaction currently holds a lock on the given row ID.
   * If so, it enters the shrinking phase and removes the row ID from the set of
   * locked rows. Then it releases the lock.
   *
   * @param rowId row ID of the released lock
   * @param lock the lock to release
   */
  void releaseLock(unsigned int rowId,
                   std::unordered_map<unsigned int, Lock*>& lockTable);

  /**
   * Checks if the transaction has a lock on the specified row.
   *
   * @param rowId
   * @returns true if it has lock, else false
   */
  auto hasLock(unsigned int rowId) -> bool;

  /**
   * Releases all the locks, that the transaction holds. This is supposed to be
   * called when the transaction aborts.
   *
   * @param lockTable containing all the locks indexed by row ID
   */
  void releaseAllLocks(std::unordered_map<unsigned int, Lock*>& lockTable);

 private:
  unsigned int transactionId_;
  bool aborted_ = false;
  std::set<unsigned int> lockedRows_;
  Phase phase_ = Phase::kGrowing;
};