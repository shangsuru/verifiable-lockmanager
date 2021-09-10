#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <thread>

#include "lock.h"
#include "transaction.h"

using libcuckoo::cuckoohash_map;

class TransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    lock_ = std::make_shared<Lock>();
    transactionA_ =
        std::make_unique<Transaction>(kTransactionIdA_, kLockBudget_);
    transaction_b_ =
        std::make_unique<Transaction>(kTransactionIdB_, kLockBudget_);
    mock_lock_table_ =
        std::make_unique<cuckoohash_map<unsigned int, std::shared_ptr<Lock>>>();
  };

  const unsigned int kTransactionIdA_ = 0;
  const unsigned int kTransactionIdB_ = 1;
  const unsigned int kLockBudget_ = 10;
  std::atomic<unsigned int> rowId_ = 0;
  std::shared_ptr<Lock> lock_;
  std::unique_ptr<Transaction> transactionA_;
  std::unique_ptr<Transaction> transaction_b_;
  std::unique_ptr<cuckoohash_map<unsigned int, std::shared_ptr<Lock>>>
      mock_lock_table_;

 public:
  void threadAcquireLock(std::unique_ptr<Transaction>& transaction,
                         unsigned int rowId) {
    auto lock = std::make_shared<Lock>();
    lock->getSharedAccess(transaction->getTransactionId());
    mock_lock_table_->insert(rowId, lock);
    transaction->addLock(rowId, Lock::LockMode::kShared, lock);
  };
};

// Several transactions can hold a shared lock together
TEST_F(TransactionTest, multipleSharedOwners) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  transaction_b_->addLock(rowId_, Lock::LockMode::kShared, lock_);

  EXPECT_TRUE(transactionA_->hasLock(rowId_));
  EXPECT_TRUE(transaction_b_->hasLock(rowId_));
};

// Cannot get exclusive access on a shared lock
TEST_F(TransactionTest, noExclusiveOnShared) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));

  try {
    transaction_b_->addLock(rowId_, Lock::LockMode::kExclusive, lock_);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Cannot get shared access on an exclusive lock
TEST_F(TransactionTest, noSharedOnExclusive) {
  transactionA_->addLock(rowId_, Lock::LockMode::kExclusive, lock_);
  EXPECT_TRUE(transactionA_->hasLock(rowId_));

  try {
    transaction_b_->addLock(rowId_, Lock::LockMode::kShared, lock_);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Enters shrinking phase after releasing a lock
TEST_F(TransactionTest, entersShrinkingPhase) {
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);

  auto locked_rows = transactionA_->getLockedRows();
  EXPECT_EQ(locked_rows.count(rowId_), 1);
  EXPECT_EQ(locked_rows.size(), 1);
  EXPECT_EQ(transactionA_->getPhase(), Transaction::Phase::kGrowing);

  transactionA_->releaseLock(rowId_, lock_);

  locked_rows = transactionA_->getLockedRows();
  EXPECT_EQ(locked_rows.count(rowId_), 0);
  EXPECT_EQ(locked_rows.size(), 0);
  EXPECT_EQ(transactionA_->getPhase(), Transaction::Phase::kShrinking);
};

// Lock budget decreases when acquiring locks
TEST_F(TransactionTest, lockBudgetDecreases) {
  auto another_lock = std::make_shared<Lock>();
  transactionA_->addLock(rowId_, Lock::LockMode::kShared, lock_);
  transactionA_->addLock(rowId_ + 1, Lock::LockMode::kExclusive, another_lock);
  EXPECT_EQ(transactionA_->getLockBudget(), kLockBudget_ - 2);

  transactionA_->releaseLock(rowId_, lock_);
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
  transactionA_->releaseAllLocks(*mock_lock_table_);

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
