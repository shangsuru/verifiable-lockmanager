#include <gtest/gtest.h>

#include <thread>

#include "lock.h"
#include "lockmanager.h"

class LockManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    spdlog::set_level(spdlog::level::off);
    lockManager_ = new LockManager();
  };

  void TearDown() override { delete lockManager_; };

  LockManager* lockManager_;
  const unsigned int kTransactionIdA = 0;
  const unsigned int kTransactionIdB = 1;
  const unsigned int kTransactionIdC = 2;
  const unsigned int kLockBudget = 10;
  const unsigned int kRowId = 0;
};

// Lock request aborts, when transaction is not registered
TEST_F(LockManagerTest, lockRequestAbortsWhenTransactionNotRegistered) {
  EXPECT_FALSE(lockManager_->lock(kTransactionIdA, kRowId, false));
};

// Registering an already registered transaction
TEST_F(LockManagerTest, cannotRegisterTwice) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_FALSE(lockManager_->registerTransaction(kTransactionIdA));
};

// Acquiring non-conflicting shared and exclusive locks works
TEST_F(LockManagerTest, acquiringLocks) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId + 1, true));
};

// Cannot get exclusive access when someone already has shared access
TEST_F(LockManagerTest, wantExclusiveButAlreadyShared) {
  lockManager_->registerTransaction(kTransactionIdA);
  lockManager_->registerTransaction(kTransactionIdB);

  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, false));
  EXPECT_FALSE(lockManager_->lock(kTransactionIdB, kRowId, true));
};

// Cannot get shared access, when someone has exclusive access
TEST_F(LockManagerTest, wantSharedButAlreadyExclusive) {
  lockManager_->registerTransaction(kTransactionIdA);
  lockManager_->registerTransaction(kTransactionIdB);

  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, true));
  EXPECT_FALSE(lockManager_->lock(kTransactionIdB, kRowId, false));
};

// Several transactions can acquire a shared lock on the same row
TEST_F(LockManagerTest, multipleTransactionsSharedLock) {
  for (unsigned int transaction_id = 0; transaction_id < kLockBudget;
       transaction_id++) {
    EXPECT_TRUE(lockManager_->registerTransaction(transaction_id));
    EXPECT_TRUE(lockManager_->lock(transaction_id, kRowId, false));
  }
};

// Cannot get the same lock twice
TEST_F(LockManagerTest, sameLockTwice) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, false));
  EXPECT_FALSE(lockManager_->lock(kTransactionIdA, kRowId, false));
};

// Can upgrade a lock
TEST_F(LockManagerTest, upgradeLock) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, true));
};

// Can unlock and acquire again
TEST_F(LockManagerTest, unlock) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdB));
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdC));

  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdB, kRowId, false));

  lockManager_->unlock(kTransactionIdA, kRowId);
  lockManager_->unlock(kTransactionIdB, kRowId);

  EXPECT_TRUE(lockManager_->lock(kTransactionIdC, kRowId, true));
};

// Cannot request more locks after transaction aborted
TEST_F(LockManagerTest, noMoreLocksAfterAbort) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, false));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId + 1, false));

  // Make transaction abort by acquiring the same lock again
  EXPECT_FALSE(lockManager_->lock(kTransactionIdA, kRowId + 1, false));

  // Make a new lock request, which should fail, because the transaction aborted
  EXPECT_FALSE(lockManager_->lock(kTransactionIdA, kRowId + 2, false));
};

// Releasing a lock twice for the same transaction has no effect
TEST_F(LockManagerTest, releaseLockTwice) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, true));
  lockManager_->unlock(kTransactionIdA, kRowId);
  lockManager_->unlock(kTransactionIdA, kRowId);
};

TEST_F(LockManagerTest, releasingAnUnownedLock) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdB));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, true));

  // Transaction B tries to unlock A's lock and acquire it
  lockManager_->unlock(kTransactionIdB, kRowId);
  EXPECT_FALSE(lockManager_->lock(kTransactionIdB, kRowId, true));
}

// Cannot acquire more locks in shrinking phase
TEST_F(LockManagerTest, noMoreLocksInShrinkingPhase) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->lock(kTransactionIdA, kRowId, true));
  // Entering shrinking phase when first releasing a lock
  lockManager_->unlock(kTransactionIdA, kRowId);
  // Cannot acquire locks in shrinking phase
  EXPECT_FALSE(lockManager_->lock(kTransactionIdA, kRowId, true));
};

// Lets two transaction request the same 2 shared locks and one of the
// transactions another exclusive lock
TEST_F(LockManagerTest, concurrentLockRequests) {
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lockManager_->registerTransaction(kTransactionIdB));

  std::thread t1{&LockManager::lock, lockManager_, kTransactionIdA, 0, false};
  std::thread t2{&LockManager::lock, lockManager_, kTransactionIdB, 0, false};
  std::thread t3{&LockManager::lock, lockManager_, kTransactionIdA, 1, false};
  std::thread t4{&LockManager::lock, lockManager_, kTransactionIdB, 1, false};
  std::thread t5{&LockManager::lock, lockManager_, kTransactionIdA, 2, false};
  std::thread t6{&LockManager::lock, lockManager_, kTransactionIdB, 2, false};
  std::thread t7{&LockManager::lock, lockManager_, kTransactionIdA, 3, false};
  std::thread t8{&LockManager::lock, lockManager_, kTransactionIdB, 3, false};
  std::thread t9{&LockManager::lock, lockManager_, kTransactionIdB, 4, true};

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
  t7.join();
  t8.join();
  t9.join();
}

// After the last lock is released, the transaction gets deleted and it can
// register again
TEST_F(LockManagerTest, transactionGetsDeletedAfterReleasingLastLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false));
  lock_manager.unlock(kTransactionIdA, kRowId);
  std::this_thread::sleep_for(std::chrono::seconds(
      1));  // need to wait here because unlock is asynchronous
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA));
}