#pragma once

#include <string_view>

#include "lock.h"

class LockManager {
 public:
  static void lock(unsigned int transactionId, unsigned int rowId,
                   Lock::LockMode lockMode);
  static void unlock(unsigned int transactionId, unsigned int rowId);
  static auto sign(unsigned int transactionId, unsigned int rowId,
                   unsigned int blockTimeout) -> std::string_view;
  static auto getLockMode(unsigned int rowId) -> Lock::LockMode;
  static auto hasLock(unsigned int transactionId, unsigned int rowId) -> bool;
  static void deleteTransaction(unsigned int transactionId);

 private:
  int privateKey_ = 0;  // TODO
  // TODO: lockTable, transactionTable
};