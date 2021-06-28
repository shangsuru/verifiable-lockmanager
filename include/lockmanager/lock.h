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
  auto getMode() -> LockMode;
  void getSharedAccess();
  void getExclusiveAccess();
  void release();
  void upgrade();

 private:
  bool exclusive_;
  unsigned int numSharedOwners_;
  std::mutex mut_;
};