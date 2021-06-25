#pragma once

#include <set>

/**
 * A Lock
 */
class Lock {
 public:
  /**
   * A lock can either be shared, then it allows multiple reads
   * or exclusive, then it allows only one write access
   */
  enum LockMode { kShared, kExclusive };
  // auto getMode() -> LockMode;
  // auto getOwners() -> std::set<Transaction>;
  // void acquire(Transaction t, LockMode mode);
  // void release(Transaction t);
  // void upgrade(Transaction t);

 private:
  bool exclusive_;
};