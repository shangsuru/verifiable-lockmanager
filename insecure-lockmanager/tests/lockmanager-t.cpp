#include <gtest/gtest.h>

#include <thread>

#include "lock.h"
#include "lockmanager.h"

class LockManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { spdlog::set_level(spdlog::level::off); };

  const unsigned int kTransactionIdA = 1;
  const unsigned int kTransactionIdB = 2;
  const unsigned int kTransactionIdC = 3;
  const unsigned int kLockBudget = 10;
  const unsigned int kRowId = 1;
};

// Lock request aborts, when transaction is not registered
TEST_F(LockManagerTest, lockRequestAbortsWhenTransactionNotRegistered) {
  LockManager lock_manager;
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, false));
};

// Registering an already registered transaction
TEST_F(LockManagerTest, cannotRegisterTwice) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_FALSE(lock_manager.registerTransaction(kTransactionIdA));
};

// Acquiring non-conflicting shared and exclusive locks works
TEST_F(LockManagerTest, acquiringLocks) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, true));
};

// Cannot get exclusive access when someone already has shared access
TEST_F(LockManagerTest, wantExclusiveButAlreadyShared) {
  LockManager lock_manager;
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.registerTransaction(kTransactionIdB);

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true));
};

// Cannot get shared access, when someone has exclusive access
TEST_F(LockManagerTest, wantSharedButAlreadyExclusive) {
  LockManager lock_manager;
  lock_manager.registerTransaction(kTransactionIdA);
  lock_manager.registerTransaction(kTransactionIdB);

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true));
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, false));
};

// Several transactions can acquire a shared lock on the same row
TEST_F(LockManagerTest, multipleTransactionsSharedLock) {
  LockManager lock_manager;
  for (unsigned int transaction_id = 0; transaction_id < kLockBudget;
       transaction_id++) {
    EXPECT_TRUE(lock_manager.registerTransaction(transaction_id));
    EXPECT_TRUE(lock_manager.lock(transaction_id, kRowId, false));
  }
};

// Cannot get the same lock twice
TEST_F(LockManagerTest, sameLockTwice) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, false));
};

// Can upgrade a lock
TEST_F(LockManagerTest, upgradeLock) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true));
};

// Can unlock and acquire again
TEST_F(LockManagerTest, unlock) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdC));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId, false));

  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdB, kRowId);

  EXPECT_TRUE(lock_manager.lock(kTransactionIdC, kRowId, true));
};

// Cannot request more locks after transaction aborted
TEST_F(LockManagerTest, noMoreLocksAfterAbort) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, false));

  // Make transaction abort by acquiring the same lock again
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId + 1, false));

  // Make a new lock request, which should fail, because the transaction aborted
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId + 2, false));
};

// Releasing a lock twice for the same transaction has no effect
TEST_F(LockManagerTest, releaseLockTwice) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true));
  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdA, kRowId);
};

TEST_F(LockManagerTest, releasingAnUnownedLock) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true));

  // Transaction B tries to unlock A's lock and acquire it
  lock_manager.unlock(kTransactionIdB, kRowId);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true));
}

// Cannot acquire more locks in shrinking phase
TEST_F(LockManagerTest, noMoreLocksInShrinkingPhase) {
  LockManager lock_manager;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true));
  // Entering shrinking phase when first releasing a lock
  lock_manager.unlock(kTransactionIdA, kRowId);
  // Cannot acquire locks in shrinking phase
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, true));
};

// After the last lock is released, the transaction gets deleted and it can
// register again
TEST_F(LockManagerTest, transactionGetsDeletedAfterReleasingLastLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  lock_manager.unlock(kTransactionIdA, kRowId);
  std::this_thread::sleep_for(std::chrono::milliseconds(
      1));  // need to wait here because unlock is asynchronous
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
}

TEST_F(LockManagerTest, notWaitingForSignature) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  for (int i = 1; i < 10000; i++) {
    lock_manager.lock(kTransactionIdA, i, false,
                      false);  // not waiting for signature return value
  }
  EXPECT_TRUE(lock_manager.lock(
      kTransactionIdA, 10000, false,
      true));  // waitung for signature return value at the end
}