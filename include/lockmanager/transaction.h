#pragma once

#include <set>

#include "lock.h"

/**
 * The internal representation of a transaction for the lock manager.
 * It keeps track of the lock budget, i.e. the maximum number of locks
 * the transaction is allowed to acquire, the set of acquired locks of
 * that transaction as well as its phase according to 2PL.
 */
class Transaction {
 public:
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
   * @returns the id identifying the transaction
   */
  auto getId() -> int;

  /**
   * @returns the set of locks the transaction curently holds
   */
  auto getLocks() -> std::set<Lock>;

  /**
   * @returns the phase the transaction is currently in, which is important for
   *          the lock manager to determine, if the transaction can acquire more
   *          locks (growing phase) or not (shrinking phase)
   */
  auto getPhase() -> Phase;

  /**
   * When the transaction acquires a new lock, it is added to the set of locks.
   *
   * @param lock newly acquired lock
   */
  void addLock(Lock lock);

  /**
   *  When the transaction releases a lock, it is removed from the set of locks.
   *
   * @param lock released lock
   */
  void deleteLock(Lock lock);

  /**
   * @returns maximum number of locks the transaction is allowed to acquire over
   *          its lifetime
   */
  auto getLockBudget() -> int;

 private:
  int transactionId_;
  std::set<Lock> locks_;
  Phase phase_;
  int lockBudget_;
};