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
    spdlog::set_level(spdlog::level::off);
    lock_ = new Lock;
    transactionA_ = std::make_unique<Transaction>(kTransactionIdA_);
    transaction_b_ = std::make_unique<Transaction>(kTransactionIdB_);
  };

  const unsigned int kTransactionIdA_ = 0;
  const unsigned int kTransactionIdB_ = 1;
  const unsigned int kLockBudget_ = 10;
  std::atomic<unsigned int> rowId_ = 0;
  Lock* lock_;
  std::unique_ptr<Transaction> transactionA_;
  std::unique_ptr<Transaction> transaction_b_;
  std::unordered_map<unsigned int, Lock*> mock_lock_table_;

 public:
  void threadAcquireLock(std::unique_ptr<Transaction>& transaction,
                         unsigned int rowId) {
    auto lock = new Lock;
    lock->getSharedAccess(transaction->getTransactionId());
    mock_lock_table_[rowId] = lock;
    transaction->addLock(rowId, Lock::LockMode::kShared, lock);
  };
};

// Several transactions can hold a shared lock together
TEST_F(TransactionTest, multipleSharedOwners) {
  EXPECT_TRUE(transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_));
  EXPECT_TRUE(transaction_b_->addLock(rowId_, Lock::LockMode::kShared, lock_));

  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_TRUE(transaction_b_->hasLock(rowId_));
};

// Cannot get exclusive access on a shared lock
TEST_F(TransactionTest, noExclusiveOnShared) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_FALSE(
      transaction_b_->addLock(rowId_, Lock::LockMode::kExclusive, lock_));
};

// Cannot get shared access on an exclusive lock
TEST_F(TransactionTest, noSharedOnExclusive) {
  transactionA_->addLock(rowId_, Lock::LockMode::kExclusive, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_FALSE(transaction_b_->addLock(rowId_, Lock::LockMode::kShared, lock_));
};

// Enters shrinking phase after releasing a lock
TEST_F(TransactionTest, entersShrinkingPhase) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  mock_lock_table_[rowId_] = lock_;

  auto locked_rows = transactionA_->getLockedRows();
  EXPECT_EQ(locked_rows.count(rowId_), 1);
  EXPECT_EQ(locked_rows.size(), 1);
  EXPECT_EQ(transactionA_->getPhase(), Transaction::Phase::kGrowing);

  transactionA_->releaseLock(rowId_, mock_lock_table_);

  locked_rows = transactionA_->getLockedRows();
  EXPECT_EQ(locked_rows.count(rowId_), 0);
  EXPECT_EQ(locked_rows.size(), 0);
  EXPECT_EQ(transactionA_->getPhase(), Transaction::Phase::kShrinking);
};

// Has lock after aquiring it
TEST_F(TransactionTest, hasLock) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));
};
