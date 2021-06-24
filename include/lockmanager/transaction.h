#pragma once

#include <set>

#include "lock.h"

// class Lock;

class Transaction {
 public:
  enum Phase { kGrowing, kShrinking };
  // auto getId() -> int;
  // auto getLocks() -> std::set<Lock>;
  // auto getPhase() -> Phase;
  // void addLock(Lock lock);
  // void deleteLock(Lock lock);
  // auto getLockBudget() -> int;

 private:
  int transactionId_;
  std::set<Lock> locks_;
  Phase phase_;
  int lockBudget_;
};