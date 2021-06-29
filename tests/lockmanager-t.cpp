#include <gtest/gtest.h>

#include "lock.h"
#include "lockmanager.h"

const unsigned int kTransactionIdA = 0;
const unsigned int kTransactionIdB = 1;
const unsigned int kTransactionIdC = 2;
const unsigned int kLockBudget = 10;
const unsigned int kRowId = 0;
LockManager lock_manager = LockManager();

// Lock request aborts, when transaction is not registered
TEST(LockManagerTest, lockRequestAbortsWhenTransactionNotRegistered) {
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
  try {
    lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
    lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction already registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Acquiring non-conflicting shared and exclusive locks works
TEST(LockManagerTest, acquiringLocks) {
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
  std::string signature_s =
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  std::string signature_x = lock_manager.lock(kTransactionIdA, kRowId + 1,
                                              Lock::LockMode::kExclusive);

  EXPECT_EQ(signature_s, "0-0S-0");
  EXPECT_EQ(signature_x, "0-1X-0");
};

// Cannot get exclusive access when someone already has shared access
TEST(LockManagerTest, wantExclusiveButAlreadyShared) {
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
  lock_manager.registerTransaction(kTransactionIdB, kLockBudget);

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
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
  lock_manager.registerTransaction(kTransactionIdB, kLockBudget);

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
  for (unsigned int transaction_id = 0; transaction_id < kLockBudget;
       transaction_id++) {
    lock_manager.registerTransaction(transaction_id, kLockBudget);
    std::string signature =
        lock_manager.lock(transaction_id, kRowId, Lock::LockMode::kShared);
    EXPECT_EQ(signature, std::to_string(transaction_id) + "-0S-0");
  }
};

// Lock budget runs out
TEST(LockManagerTest, lockBudgetRunsOut) {
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);

  unsigned int row_id = 0;
  for (; row_id < kLockBudget; row_id++) {
    std::string signature =
        lock_manager.lock(kTransactionIdA, row_id, Lock::LockMode::kShared);
    std::string expected_signature = "0-" + std::to_string(row_id) + "S-0";
    EXPECT_EQ(signature, expected_signature);
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
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
  std::string signature_a =
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  std::string signature_b =
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);

  EXPECT_EQ(signature_a, "0-0S-0");
  EXPECT_EQ(signature_b, "0-0X-0");
};

// Can unlock and acquire again
TEST(LockManagerTest, unlock) {
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
  lock_manager.registerTransaction(kTransactionIdB, kLockBudget);
  lock_manager.registerTransaction(kTransactionIdC, kLockBudget);

  std::string signature_a =
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared);
  std::string signature_b =
      lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kShared);

  EXPECT_EQ(signature_a, "0-0S-0");
  EXPECT_EQ(signature_b, "1-0S-0");

  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdB, kRowId);

  std::string signature_c =
      lock_manager.lock(kTransactionIdC, kRowId, Lock::LockMode::kExclusive);

  EXPECT_EQ(signature_c, "2-0X-0");
};

// Cannot request more locks after transaction aborted
TEST(LockManagerTest, noMoreLocksAfterAbort) {
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
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
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
  lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive);
  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdA, kRowId);
};

// Cannot acquire more locks in shrinking phase
TEST(LockManagerTest, noMoreLocksInShrinkingPhase) {
  lock_manager.registerTransaction(kTransactionIdA, kLockBudget);
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
