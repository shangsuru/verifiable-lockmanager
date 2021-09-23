#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <thread>

#include "hashtable.h"
#include "lock.h"
#include "transaction.h"

class TransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    lock_ = newLock();
    transactionA_ = newTransaction(kTransactionIdA_, kLockBudget_);
    transactionB_ = newTransaction(kTransactionIdB_, kLockBudget_);
    lockTable_ = newHashTable(100);
  };

  void TearDown() override {
    delete transactionA_;
    delete transactionB_;
  }

  const unsigned int kTransactionIdA_ = 0;
  const unsigned int kTransactionIdB_ = 1;
  const unsigned int kLockBudget_ = 10;
  std::atomic<unsigned int> rowId_ = 0;
  Lock* lock_;
  Transaction* transactionA_;
  Transaction* transactionB_;
  HashTable* lockTable_;

 public:
  void acquireLock(Transaction* transaction, unsigned int rowId) {
    auto lock = newLock();
    getSharedAccess(lock, transaction->transaction_id);
    set(lockTable_, rowId, (void*)lock);
    addLock(transaction, rowId, false, lock);
  };
};

// Several transactions can hold a shared lock together
TEST_F(TransactionTest, multipleSharedOwners) {
  addLock(transactionA_, rowId_, false, lock_);
  addLock(transactionB_, rowId_, false, lock_);

  EXPECT_TRUE(hasLock(transactionA_, rowId_));
  EXPECT_TRUE(hasLock(transactionB_, rowId_));
};

// Cannot get exclusive access on a shared lock
TEST_F(TransactionTest, noExclusiveOnShared) {
  addLock(transactionA_, rowId_, false, lock_);
  EXPECT_TRUE(hasLock(transactionA_, rowId_));
  EXPECT_FALSE(addLock(transactionB_, rowId_, true, lock_));
};

// Cannot get shared access on an exclusive lock
TEST_F(TransactionTest, noSharedOnExclusive) {
  addLock(transactionA_, rowId_, true, lock_);
  EXPECT_TRUE(hasLock(transactionA_, rowId_));
  EXPECT_FALSE(addLock(transactionB_, rowId_, false, lock_));
};

// Enters shrinking phase after releasing a lock
TEST_F(TransactionTest, entersShrinkingPhase) {
  EXPECT_TRUE(addLock(transactionA_, rowId_, false, lock_));
  set(lockTable_, rowId_, (void*)lock_);

  auto locked_rows = transactionA_->locked_rows;
  EXPECT_EQ(transactionA_->num_locked, 1);
  EXPECT_EQ(locked_rows[0], kTransactionIdA_);
  EXPECT_TRUE(transactionA_->growing_phase);

  releaseLock(transactionA_, rowId_, lockTable_);

  EXPECT_EQ(transactionA_->num_locked, 0);
  EXPECT_FALSE(transactionA_->growing_phase);
};

// Lock budget decreases when acquiring locks
TEST_F(TransactionTest, lockBudgetDecreases) {
  auto another_lock = newLock();
  addLock(transactionA_, rowId_, false, lock_);
  set(lockTable_, rowId_, (void*)lock_);
  addLock(transactionA_, rowId_ + 1, true, another_lock);
  set(lockTable_, rowId_ + 1, (void*)another_lock);
  EXPECT_EQ(transactionA_->lock_budget, kLockBudget_ - 2);

  releaseLock(transactionA_, rowId_, lockTable_);
  EXPECT_EQ(transactionA_->lock_budget, kLockBudget_ - 2);
};

// Has lock after aquiring it
TEST_F(TransactionTest, hasLock) {
  addLock(transactionA_, rowId_, false, lock_);
  EXPECT_TRUE(hasLock(transactionA_, rowId_));
};

// Transaction releases all locks under concurrent lock requests
TEST_F(TransactionTest, releasesAllLocks) {
  // Start multiple threads that add Locks for that transaction
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);

  // Release all locks
  releaseAllLocks(transactionA_, lockTable_);

  // Start more threads that add locks for that transaction
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);
  acquireLock(transactionA_, rowId_++);

  // Assert that the transaction holds no locks
  EXPECT_EQ(transactionA_->num_locked, 0);
};