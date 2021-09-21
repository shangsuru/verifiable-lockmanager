#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <thread>
#include <unordered_map>

#include "lock.h"
#include "transaction.h"

class TransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    lock_ = new Lock();
    transactionA_ = new Transaction(kTransactionIdA_, kLockBudget_);
    transactionB_ = new Transaction(kTransactionIdB_, kLockBudget_);
    lockTable_ = std::unordered_map<unsigned int, Lock*>();
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
  std::unordered_map<unsigned int, Lock*> lockTable_;

 public:
  void threadAcquireLock(Transaction* transaction, unsigned int rowId) {
    auto lock = new Lock();
    lock->getSharedAccess(transaction->getTransactionId());
    lockTable_[rowId] = lock;
    transaction->addLock(rowId, Lock::LockMode::kShared, lock);
  };
};

// Several transactions can hold a shared lock together
TEST_F(TransactionTest, multipleSharedOwners) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  transactionB_->addLock(rowId_, Lock::LockMode::kShared, lock_);

  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_TRUE(transactionB_->hasLock(rowId_));
};

// Cannot get exclusive access on a shared lock
TEST_F(TransactionTest, noExclusiveOnShared) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_FALSE(
      transactionB_->addLock(rowId_, Lock::LockMode::kExclusive, lock_));
};

// Cannot get shared access on an exclusive lock
TEST_F(TransactionTest, noSharedOnExclusive) {
  transactionA_->addLock(rowId_, Lock::LockMode::kExclusive, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_FALSE(transactionB_->addLock(rowId_, Lock::LockMode::kShared, lock_));
};

// Enters shrinking phase after releasing a lock
TEST_F(TransactionTest, entersShrinkingPhase) {
  EXPECT_TRUE(transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_));
  lockTable_[rowId_] = lock_;

  auto locked_rows = transactionA_->getLockedRows();
  EXPECT_EQ(locked_rows.count(rowId_), 1);
  EXPECT_EQ(locked_rows.size(), 1);
  EXPECT_EQ(transactionA_->getPhase(), Transaction::Phase::kGrowing);

  transactionA_->releaseLock(rowId_, lockTable_);

  locked_rows = transactionA_->getLockedRows();
  EXPECT_EQ(locked_rows.count(rowId_), 0);
  EXPECT_EQ(locked_rows.size(), 0);
  EXPECT_EQ(transactionA_->getPhase(), Transaction::Phase::kShrinking);
};

// Lock budget decreases when acquiring locks
TEST_F(TransactionTest, lockBudgetDecreases) {
  auto another_lock = new Lock();
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  lockTable_[rowId_] = lock_;
  transactionA_->addLock(rowId_ + 1, Lock::LockMode::kExclusive, another_lock);
  lockTable_[rowId_ + 1] = another_lock;
  EXPECT_EQ(transactionA_->getLockBudget(), kLockBudget_ - 2);

  transactionA_->releaseLock(rowId_, lockTable_);
  EXPECT_EQ(transactionA_->getLockBudget(), kLockBudget_ - 2);
};

// Has lock after aquiring it
TEST_F(TransactionTest, hasLock) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));
};

// Transaction releases all locks under concurrent lock requests
TEST_F(TransactionTest, releasesAllLocks) {
  // Start multiple threads that add Locks for that transaction
  std::thread t1{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t2{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t3{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t4{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t5{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t6{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};

  // Release all locks
  transactionA_->releaseAllLocks(lockTable_);

  // Start more threads that add locks for that transaction
  std::thread t7{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t8{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t9{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t10{&TransactionTest::threadAcquireLock, this,
                  std::ref(transactionA_), rowId_++};
  std::thread t11{&TransactionTest::threadAcquireLock, this,
                  std::ref(transactionA_), rowId_++};
  std::thread t12{&TransactionTest::threadAcquireLock, this,
                  std::ref(transactionA_), rowId_++};

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
  t7.join();
  t8.join();
  t9.join();
  t10.join();
  t11.join();
  t12.join();

  // Assert that the transaction holds no locks
  EXPECT_EQ(transactionA_->getLockedRows().size(), 0);
};

// Add locks concurrently
TEST_F(TransactionTest, concurrentAddLock) {
  // Start multiple threads that acquire Locks for that transaction
  std::thread t1{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t2{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t3{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t4{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t5{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};
  std::thread t6{&TransactionTest::threadAcquireLock, this,
                 std::ref(transactionA_), rowId_++};

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();

  EXPECT_EQ(transactionA_->getLockBudget(), kLockBudget_ - 6);
  EXPECT_EQ(transactionA_->getLockedRows().size(), 6);
};
