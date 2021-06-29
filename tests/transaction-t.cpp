#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <thread>

#include "lock.h"
#include "transaction.h"

using libcuckoo::cuckoohash_map;

const unsigned int kTransactionIdA = 0;
const unsigned int kTransactionIdB = 1;
const unsigned int kLockBudget = 10;
std::atomic<unsigned int> row_id = 0;
auto lock = std::make_shared<Lock>();
Transaction transaction_a = Transaction(kTransactionIdA, kLockBudget);
Transaction transaction_b = Transaction(kTransactionIdB, kLockBudget);
cuckoohash_map<unsigned int, std::shared_ptr<Lock>> mock_lock_table;

void threadAcquireLock(Transaction& transaction) {
  auto lock = std::make_shared<Lock>();
  transaction.addLock(++row_id, Lock::LockMode::kShared, lock);
};

// Several transactions can hold a shared lock together
TEST(TransactionTest, multipleSharedOwners) {
  transaction_a.addLock(row_id, Lock::LockMode::kShared, lock);
  transaction_b.addLock(row_id, Lock::LockMode::kShared, lock);

  EXPECT_TRUE(transaction_a.hasLock(row_id));
  EXPECT_TRUE(transaction_b.hasLock(row_id));
};

// Cannot get exclusive access on a shared lock
TEST(TransactionTest, noExclusiveOnShared) {
  transaction_a.addLock(row_id, Lock::LockMode::kShared, lock);
  EXPECT_TRUE(transaction_a.hasLock(row_id));

  try {
    transaction_b.addLock(row_id, Lock::LockMode::kExclusive, lock);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Cannot get shared access on an exclusive lock
TEST(TransactionTest, noSharedOnExclusive) {
  transaction_a.addLock(row_id, Lock::LockMode::kExclusive, lock);
  EXPECT_TRUE(transaction_a.hasLock(row_id));

  try {
    transaction_b.addLock(row_id, Lock::LockMode::kShared, lock);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Enters shrinking phase after releasing a lock
TEST(TransactionTest, entersShrinkingPhase) {
  transaction_a.addLock(row_id, Lock::LockMode::kShared, lock);

  auto locked_rows = transaction_a.getLockedRows();
  EXPECT_EQ(locked_rows.count(row_id), 1);
  EXPECT_EQ(locked_rows.size(), 1);
  EXPECT_EQ(transaction_a.getPhase(), Transaction::Phase::kGrowing);

  transaction_a.releaseLock(row_id, lock);

  locked_rows = transaction_a.getLockedRows();
  EXPECT_EQ(locked_rows.count(row_id), 0);
  EXPECT_EQ(locked_rows.size(), 0);
  EXPECT_EQ(transaction_a.getPhase(), Transaction::Phase::kShrinking);
};

// Lock budget decreases when acquiring locks
TEST(TransactionTest, lockBudgetDecreases) {
  auto another_lock = std::make_shared<Lock>();
  transaction_a.addLock(row_id, Lock::LockMode::kShared, lock);
  transaction_a.addLock(row_id + 1, Lock::LockMode::kExclusive, another_lock);
  EXPECT_EQ(transaction_a.getLockBudget(), kLockBudget - 2);

  transaction_a.releaseLock(row_id, lock);
  EXPECT_EQ(transaction_a.getLockBudget(), kLockBudget - 2);
};

// Has lock after aquiring it
TEST(TransactionTest, hasLock) {
  transaction_a.addLock(row_id, Lock::LockMode::kShared, lock);
  EXPECT_TRUE(transaction_a.hasLock(row_id));
};

// Transaction releases all locks under concurrent lock requests
TEST(DISABLED_TransactionTest, releasesAllLocks) {
  // Start multiple threads that add Locks for that transaction
  std::thread t1{threadAcquireLock, std::ref(transaction_a)};
  std::thread t2{threadAcquireLock, std::ref(transaction_a)};
  std::thread t3{threadAcquireLock, std::ref(transaction_a)};
  std::thread t4{threadAcquireLock, std::ref(transaction_a)};
  std::thread t5{threadAcquireLock, std::ref(transaction_a)};
  std::thread t6{threadAcquireLock, std::ref(transaction_a)};

  // Release all locks
  transaction_a.releaseAllLocks(mock_lock_table);

  // Start more threads that add locks for that transaction
  std::thread t7{threadAcquireLock, std::ref(transaction_a)};
  std::thread t8{threadAcquireLock, std::ref(transaction_a)};
  std::thread t9{threadAcquireLock, std::ref(transaction_a)};
  std::thread t10{threadAcquireLock, std::ref(transaction_a)};
  std::thread t11{threadAcquireLock, std::ref(transaction_a)};
  std::thread t12{threadAcquireLock, std::ref(transaction_a)};

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
};

// Add locks concurrently
TEST(DISABLED_TransactionTest, concurrentAddLock) {
  // Start multiple threads that acquire Locks for that transaction
  std::thread t1{threadAcquireLock, std::ref(transaction_a)};
  std::thread t2{threadAcquireLock, std::ref(transaction_a)};
  std::thread t3{threadAcquireLock, std::ref(transaction_a)};
  std::thread t4{threadAcquireLock, std::ref(transaction_a)};
  std::thread t5{threadAcquireLock, std::ref(transaction_a)};
  std::thread t6{threadAcquireLock, std::ref(transaction_a)};

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();

  EXPECT_EQ(transaction_a.getLockBudget(), kLockBudget - 6);
  EXPECT_EQ(transaction_a.getLockedRows().size(), 6);
};
