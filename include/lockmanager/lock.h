#pragma once

#include <set>

// #include "transaction.h"

class Lock {
 public:
  enum LockMode { kShared, kExclusive };
  // auto getMode() -> LockMode;
  // auto getOwners() -> std::set<Transaction>;
  // void acquire(Transaction t, LockMode mode);
  // void release(Transaction t);
  // void upgrade(Transaction t);

 private:
  // std::set<Transaction> owners_;
  bool exclusive_;
};