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
    transaction_a_ =
        std::make_unique<Transaction>(kTransactionIdA_, kLockBudget_);
    transaction_b_ =
        std::make_unique<Transaction>(kTransactionIdB_, kLockBudget_);
    mock_lock_table_ =
        std::make_unique<cuckoohash_map<unsigned int, std::shared_ptr<Lock>>>();
  };

  const unsigned int kTransactionIdA_ = 0;
  const unsigned int kTransactionIdB_ = 1;
  const unsigned int kLockBudget_ = 10;
  std::atomic<unsigned int> row_id_ = 0;
  std::shared_ptr<Lock> lock_;
  std::unique_ptr<Transaction> transaction_a_;
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
  transaction_a_->addLock(row_id_, Lock::LockMode::kShared, lock_);
  transaction_b_->addLock(row_id_, Lock::LockMode::kShared, lock_);

  EXPECT_TRUE(transaction_a_->hasLock(row_id_));
  EXPECT_TRUE(transaction_b_->hasLock(row_id_));
};

// Cannot get exclusive access on a shared lock
TEST_F(TransactionTest, noExclusiveOnShared) {
  transaction_a_->addLock(row_id_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transaction_a_->hasLock(row_id_));

  try {
    transaction_b_->addLock(row_id_, Lock::LockMode::kExclusive, lock_);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Cannot get shared access on an exclusive lock
TEST_F(TransactionTest, noSharedOnExclusive) {
  transaction_a_->addLock(row_id_, Lock::LockMode::kExclusive, lock_);
  EXPECT_TRUE(transaction_a_->hasLock(row_id_));

  try {
    transaction_b_->addLock(row_id_, Lock::LockMode::kShared, lock_);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Enters shrinking phase after releasing a lock
TEST_F(TransactionTest, entersShrinkingPhase) {
  transaction_a_->addLock(row_id_, Lock::LockMode::kShared, lock_);

  auto locked_rows = transaction_a_->getLockedRows();
  EXPECT_EQ(locked_rows.count(row_id_), 1);
  EXPECT_EQ(locked_rows.size(), 1);
  EXPECT_EQ(transaction_a_->getPhase(), Transaction::Phase::kGrowing);

  transaction_a_->releaseLock(row_id_, lock_);

  locked_rows = transaction_a_->getLockedRows();
  EXPECT_EQ(locked_rows.count(row_id_), 0);
  EXPECT_EQ(locked_rows.size(), 0);
  EXPECT_EQ(transaction_a_->getPhase(), Transaction::Phase::kShrinking);
};

// Lock budget decreases when acquiring locks
TEST_F(TransactionTest, lockBudgetDecreases) {
  auto another_lock = std::make_shared<Lock>();
  transaction_a_->addLock(row_id_, Lock::LockMode::kShared, lock_);
  transaction_a_->addLock(row_id_ + 1, Lock::LockMode::kExclusive,
                          another_lock);
  EXPECT_EQ(transaction_a_->getLockBudget(), kLockBudget_ - 2);

  transaction_a_->releaseLock(row_id_, lock_);
  EXPECT_EQ(transaction_a_->getLockBudget(), kLockBudget_ - 2);
};

// Has lock after aquiring it
TEST_F(TransactionTest, hasLock) {
  transaction_a_->addLock(row_id_, Lock::LockMode::kShared, lock_);
  EXPECT_TRUE(transaction_a_->hasLock(row_id_));
};
/*
// Transaction releases all locks under concurrent lock requests
TEST_F(TransactionTest, releasesAllLocks) {
  // Start multiple threads that add Locks for that transaction
  std::thread t1{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 1};
  std::thread t2{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 2};
  std::thread t3{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 3};
  std::thread t4{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 4};
  std::thread t5{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 5};
  std::thread t6{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 6};

  // Release all locks
  transaction_a->releaseAllLocks(*mock_lock_table.get());

  // Start more threads that add locks for that transaction
  std::thread t7{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 7};
  std::thread t8{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 8};
  std::thread t9{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 9};
  std::thread t10{&TransactionTest::threadAcquireLock, this,
                  std::ref(transaction_a), 10};
  std::thread t11{&TransactionTest::threadAcquireLock, this,
                  std::ref(transaction_a), 11};
  std::thread t12{&TransactionTest::threadAcquireLock, this,
                  std::ref(transaction_a), 12};

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
  EXPECT_EQ(transaction_a->getLockedRows().size(), 0);
};

// Add locks concurrently
TEST_F(TransactionTest, concurrentAddLock) {
  // Start multiple threads that acquire Locks for that transaction
  std::thread t1{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 1};
  std::thread t2{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 2};
  std::thread t3{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 3};
  std::thread t4{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 4};
  std::thread t5{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 5};
  std::thread t6{&TransactionTest::threadAcquireLock, this,
                 std::ref(transaction_a), 6};

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();

  EXPECT_EQ(transaction_a->getLockBudget(), kLockBudget - 6);
  EXPECT_EQ(transaction_a->getLockedRows().size(), 6);
};*/
