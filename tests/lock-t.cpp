#include <gtest/gtest.h>

#include <thread>

#include "lock.h"

Lock lock = Lock();
const unsigned int kTransactionIdA = 0;
const unsigned int kTransactionIdB = 1;

void threadSharedAccess(unsigned int id) { lock.getSharedAccess(id); }

void threadRelease(unsigned int id) { lock.release(id); }

// Shared access works
TEST(LockTest, sharedAccess) {
  std::thread t1{threadSharedAccess, 1};
  std::thread t2{threadSharedAccess, 2};
  std::thread t3{threadSharedAccess, 3};
  std::thread t4{threadSharedAccess, 4};

  t1.join();
  t2.join();
  t3.join();
  t4.join();

  EXPECT_EQ(lock.getMode(), Lock::LockMode::kShared);
  EXPECT_EQ(lock.getOwners().size(), 4);
};

// Exclusive access works
TEST(LockTest, exclusiveAccess) {
  lock.getExclusiveAccess(kTransactionIdA);
  EXPECT_EQ(lock.getMode(), Lock::LockMode::kExclusive);
  auto owners = lock.getOwners();
  EXPECT_EQ(owners.count(kTransactionIdA), 1);
  EXPECT_EQ(owners.size(), 1);
}

// Cannot acquire shared access on an exclusive lock
TEST(LockTest, noSharedOnExclusive) {
  lock.getExclusiveAccess(kTransactionIdA);
  try {
    lock.getSharedAccess(kTransactionIdB);
    FAIL() << "Expected std::domain_error";
  } catch (std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Cannot get exclusive access on an exclusive lock
TEST(LockTest, noExclusiveOnExclusive) {
  lock.getExclusiveAccess(kTransactionIdA);
  try {
    lock.getExclusiveAccess(kTransactionIdB);
    FAIL() << "Expected std::domain_error";
  } catch (std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Cannot acquire exclusive access on a shared lock
TEST(LockTest, noExclusiveOnShared) {
  lock.getSharedAccess(kTransactionIdA);
  try {
    lock.getExclusiveAccess(kTransactionIdB);
    FAIL() << "Expected std::domain_error";
  } catch (std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Upgrade
TEST(LockTest, upgrade) {
  lock.getSharedAccess(kTransactionIdA);
  lock.upgrade(kTransactionIdA);
  auto owners = lock.getOwners();
  EXPECT_EQ(lock.getMode(), Lock::LockMode::kExclusive);
  EXPECT_EQ(owners.count(kTransactionIdA), 1);
  EXPECT_EQ(owners.size(), 1);
}

// Concurrently adding and releasing locks
TEST(LockTest, concurrentlyAddingAndReleasingLocks) {
  lock.getSharedAccess(1);
  lock.getSharedAccess(2);
  lock.getSharedAccess(3);
  lock.getSharedAccess(4);

  std::thread t1{threadRelease, 1};
  std::thread t2{threadSharedAccess, 0};
  std::thread t3{threadRelease, 3};

  t1.join();
  t2.join();
  t3.join();

  auto owners = lock.getOwners();
  EXPECT_EQ(owners.size(), 3);
  EXPECT_EQ(owners.count(2), 1);
  EXPECT_EQ(owners.count(4), 1);
  EXPECT_EQ(owners.count(0), 1);
}