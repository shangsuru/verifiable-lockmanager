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
  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
};

// Registering an already registered transaction
TEST(LockManagerTest, cannotRegisterTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_FALSE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
};

// Acquiring non-conflicting shared and exclusive locks works
TEST(LockManagerTest, acquiringLocks) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId + 1, Lock::LockMode::kExclusive)
          .second);
};

// Cannot get exclusive access when someone already has shared access
TEST(LockManagerTest, wantExclusiveButAlreadyShared) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kExclusive)
          .second);
};

// Cannot get shared access, when someone has exclusive access
TEST(LockManagerTest, wantSharedButAlreadyExclusive) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive)
          .second);
  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kShared)
          .second);
};

// Several transactions can acquire a shared lock on the same row
TEST(LockManagerTest, multipleTransactionsSharedLock) {
  LockManager lock_manager = LockManager();
  for (unsigned int transaction_id = 0; transaction_id < kLockBudget;
       transaction_id++) {
    EXPECT_TRUE(lock_manager.registerTransaction(transaction_id, kLockBudget));
    EXPECT_TRUE(
        lock_manager.lock(transaction_id, kRowId, Lock::LockMode::kShared)
            .second);
  }
};

// Cannot get the same lock twice
TEST(LockManagerTest, sameLockTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
};

// Lock budget runs out
TEST(LockManagerTest, lockBudgetRunsOut) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));

  unsigned int row_id = 0;
  for (; row_id < kLockBudget; row_id++) {
    EXPECT_TRUE(
        lock_manager.lock(kTransactionIdA, row_id, Lock::LockMode::kShared)
            .second);
    std::string expected_signature = "0-" + std::to_string(row_id) + "S-0";
  }

  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdA, row_id + 1, Lock::LockMode::kShared)
          .second);
};

// Can upgrade a lock
TEST(LockManagerTest, upgradeLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive)
          .second);
};

// Can unlock and acquire again
TEST(LockManagerTest, unlock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdC, kLockBudget));

  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdB, kRowId, Lock::LockMode::kShared)
          .second);

  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdB, kRowId);

  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdC, kRowId, Lock::LockMode::kExclusive)
          .second);
};

// Cannot request more locks after transaction aborted
TEST(LockManagerTest, noMoreLocksAfterAbort) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kShared)
          .second);
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId + 1, Lock::LockMode::kShared)
          .second);

  // Make transaction abort by acquiring the same lock again
  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdA, kRowId + 1, Lock::LockMode::kShared)
          .second);

  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdA, kRowId + 2, Lock::LockMode::kShared)
          .second);
};

// Releasing a lock twice for the same transaction has no effect
TEST(LockManagerTest, releaseLockTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive)
          .second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdA, kRowId);
};

// Cannot acquire more locks in shrinking phase
TEST(LockManagerTest, noMoreLocksInShrinkingPhase) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive)
          .second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  EXPECT_FALSE(
      lock_manager.lock(kTransactionIdA, kRowId, Lock::LockMode::kExclusive)
          .second);
};
