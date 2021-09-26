#include <gtest/gtest.h>

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
  LockManager lock_manager = LockManager();
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
};

// Registering an already registered transaction
TEST_F(LockManagerTest, cannotRegisterTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_FALSE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
};

// Acquiring non-conflicting shared and exclusive locks works
TEST_F(LockManagerTest, acquiringLocks) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, true).second);
};

// Cannot get exclusive access when someone already has shared access
TEST_F(LockManagerTest, wantExclusiveButAlreadyShared) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
};

// Cannot get shared access, when someone has exclusive access
TEST_F(LockManagerTest, wantSharedButAlreadyExclusive) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, false).second);
};

// Several transactions can acquire a shared lock on the same row
TEST_F(LockManagerTest, multipleTransactionsSharedLock) {
  LockManager lock_manager = LockManager();
  for (unsigned int transaction_id = 1; transaction_id < kLockBudget;
       transaction_id++) {
    EXPECT_TRUE(lock_manager.registerTransaction(transaction_id, kLockBudget));
    EXPECT_TRUE(lock_manager.lock(transaction_id, kRowId, false).second);
  }
};

// Cannot get the same lock twice
TEST_F(LockManagerTest, sameLockTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
};

// Lock budget runs out
TEST_F(LockManagerTest, lockBudgetRunsOut) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));

  unsigned int row_id = 0;
  for (; row_id < kLockBudget; row_id++) {
    EXPECT_TRUE(lock_manager.lock(kTransactionIdA, row_id, false).second);
    std::string expected_signature = "0-" + std::to_string(row_id) + "S-0";
  }

  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, row_id + 1, false).second);
};

// Can upgrade a lock
TEST_F(LockManagerTest, upgradeLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
};

// Can unlock and acquire again
TEST_F(LockManagerTest, unlock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
};

// Cannot request more locks after transaction aborted
TEST_F(LockManagerTest, noMoreLocksAfterAbort) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, false).second);

  // Make transaction abort by acquiring the same lock again
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId + 1, false).second);

  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId + 2, false).second);
};

// Releasing a lock twice for the same transaction has no effect
TEST_F(LockManagerTest, releaseLockTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdA, kRowId);
};

TEST_F(LockManagerTest, releasingAnUnownedLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);

  // Transaction B tries to unlock A's lock and acquire it
  lock_manager.unlock(kTransactionIdB, kRowId);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
}

// Cannot acquire more locks in shrinking phase
TEST_F(LockManagerTest, noMoreLocksInShrinkingPhase) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
};

TEST_F(LockManagerTest, verifySignature) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  std::string signature =
      lock_manager.lock(kTransactionIdA, kRowId, true).first;
  EXPECT_TRUE(lock_manager.verify_signature_string(signature, kTransactionIdA,
                                                   kRowId, true));
}

TEST_F(LockManagerTest, abortedTransactionCanRegisterAgain) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, true)
                   .second);  // makes A abort
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
}
