#include <gtest/gtest.h>

#include <thread>

#include "lock.h"

class LockTest : public ::testing::Test {
 protected:
  void SetUp() override { spdlog::set_level(spdlog::level::off); };

  const unsigned int kTransactionIdA_ = 0;
  const unsigned int kTransactionIdB_ = 1;
};

// Shared access works
TEST_F(LockTest, sharedAccess) {
  Lock lock = Lock();
  lock.getSharedAccess(1);
  lock.getSharedAccess(2);
  lock.getSharedAccess(3);
  lock.getSharedAccess(4);

  EXPECT_EQ(lock.getMode(), Lock::LockMode::kShared);
  EXPECT_EQ(lock.getOwners().size(), 4);
};

// Exclusive access works
TEST_F(LockTest, exclusiveAccess) {
  Lock lock = Lock();
  lock.getExclusiveAccess(kTransactionIdA_);
  EXPECT_EQ(lock.getMode(), Lock::LockMode::kExclusive);
  auto owners = lock.getOwners();
  EXPECT_EQ(owners.count(kTransactionIdA_), 1);
  EXPECT_EQ(owners.size(), 1);
}

// Cannot acquire shared access on an exclusive lock
TEST_F(LockTest, noSharedOnExclusive) {
  Lock lock = Lock();
  EXPECT_TRUE(lock.getExclusiveAccess(kTransactionIdA_));
  EXPECT_FALSE(lock.getSharedAccess(kTransactionIdB_));
};

// Cannot get exclusive access on an exclusive lock
TEST_F(LockTest, noExclusiveOnExclusive) {
  Lock lock = Lock();
  EXPECT_TRUE(lock.getExclusiveAccess(kTransactionIdA_));
  EXPECT_FALSE(lock.getExclusiveAccess(kTransactionIdB_));
};

// Cannot acquire exclusive access on a shared lock
TEST_F(LockTest, noExclusiveOnShared) {
  Lock lock = Lock();
  EXPECT_TRUE(lock.getSharedAccess(kTransactionIdA_));
  EXPECT_FALSE(lock.getExclusiveAccess(kTransactionIdB_));
};

// Upgrade
TEST_F(LockTest, upgrade) {
  Lock lock = Lock();
  lock.getSharedAccess(kTransactionIdA_);
  lock.upgrade(kTransactionIdA_);
  auto owners = lock.getOwners();
  EXPECT_EQ(lock.getMode(), Lock::LockMode::kExclusive);
  EXPECT_EQ(owners.count(kTransactionIdA_), 1);
  EXPECT_EQ(owners.size(), 1);
}