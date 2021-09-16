#include <gtest/gtest.h>

#include "lock.h"
#include "lockmanager.h"

const unsigned int kTransactionIdA = 0;
const unsigned int kTransactionIdB = 1;
const unsigned int kTransactionIdC = 2;
const unsigned int kLockBudget = 10;
const unsigned int kRowId = 0;

// Lock request aborts, when transaction is not registered
TEST(LockManagerTest, lockRequestAbortsWhenTransactionNotRegistered) {
  LockManager lock_manager = LockManager();
  try {
    lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction was not registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Registering an already registered transaction
TEST(LockManagerTest, cannotRegisterTwice) {
  LockManager lock_manager = LockManager();
  try {
    lock_manager.registerTransaction(kTransactionIdA);
    lock_manager.registerTransaction(kTransactionIdA);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction already registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Acquiring non-conflicting shared and exclusive locks works
TEST(LockManagerTest, acquiringLocks) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  lock_manager.lock(kTransactionIdA, kRowId + 1, Lock::LockMode::kExclusive);
};

// Cannot get exclusive access when someone already has shared access
TEST(LockManagerTest, wantExclusiveButAlreadyShared) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.registerTransaction(kTransactionIdB);

  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  try {
    lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kExclusive);
    FAIL() << "Expected std::domain error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain error";
  }
};

// Cannot get shared access, when someone has exclusive access
TEST(LockManagerTest, wantSharedButAlreadyExclusive) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.registerTransaction(kTransactionIdB);

  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);
  try {
    lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain error";
  }
};

// Several transactions can acquire a shared lock on the same row
TEST(LockManagerTest, multipleTransactionsSharedLock) {
  LockManager lock_manager = LockManager();
  for (unsigned int transaction_id = 0; transaction_id < kLockBudget;
       transaction_id++) {
    lock_manager.registerTransaction(transaction_id);
    lock_manager.lock(transaction_id, kRowId, Lock::LockMode::kShared);
  }
};

// Cannot get the same lock twice
TEST(LockManagerTest, sameLockTwice) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  try {
    lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request for already acquired lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Lock budget runs out
TEST(LockManagerTest, lockBudgetRunsOut) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);

  unsigned int row_id = 0;
  for (; row_id < kLockBudget; row_id++) {
    lock_manager.lock(kTransactionIdA, row_id, Lock::LockMode::kShared);
    std::string expected_signature = "0-" + std::to_string(row_id) + "S-0";
  }

  try {
    lock_manager.lock(kTransactionIdA, row_id + 1, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Lock budget exhausted"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Can upgrade a lock
TEST(LockManagerTest, upgradeLock) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);
};

// Can unlock and acquire again
TEST(LockManagerTest, unlock) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.registerTransaction(kTransactionIdB);
  lock_manager.registerTransaction(kTransactionIdC);

  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kShared);

  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdB, kRowId);

  lock_manager.lock(kTransactionIdC, kRowId, Lock::LockMode::kExclusive);
};

// Cannot request more locks after transaction aborted
TEST(LockManagerTest, noMoreLocksAfterAbort) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  lock_manager.lock(kTransactionIdA, kRowId + 1, Lock::LockMode::kShared);

  // Make transaction abort by acquiring the same lock again
  try {
    lock_manager.lock(kTransactionIdA, kRowId + 1, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request for already acquired lock"));
  }

  try {
    lock_manager.lock(kTransactionIdA, kRowId + 2, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction was not registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Releasing a lock twice for the same transaction has no effect
TEST(LockManagerTest, releaseLockTwice) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);
  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdA, kRowId);
};

// Cannot acquire more locks in shrinking phase
TEST(LockManagerTest, noMoreLocksInShrinkingPhase) {
  LockManager lock_manager = LockManager();
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);
  lock_manager.unlock(kTransactionIdA, kRowId);
  try {
    lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(),
              std::string("Cannot acquire more locks according to 2PL"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};
