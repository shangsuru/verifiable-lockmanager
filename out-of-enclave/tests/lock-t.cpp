#include <gtest/gtest.h>

#include <thread>

#include "lock.h"

const unsigned int kTransactionIdA = 0;
const unsigned int kTransactionIdB = 1;

// Shared access works
TEST(LockTest, sharedAccess) {
  Lock* lock = newLock();
  getSharedAccess(lock, 1);
  getSharedAccess(lock, 2);
  getSharedAccess(lock, 3);
  getSharedAccess(lock, 4);

  EXPECT_FALSE(lock->exclusive);
  EXPECT_EQ(lock->num_owners, 4);
};

// Exclusive access works
TEST(LockTest, exclusiveAccess) {
  Lock* lock = newLock();
  getExclusiveAccess(lock, kTransactionIdA);
  EXPECT_TRUE(lock->exclusive);
  EXPECT_EQ(lock->num_owners, 1);
  EXPECT_EQ(lock->owners[0], kTransactionIdA);
}

// Cannot acquire shared access on an exclusive lock
TEST(LockTest, noSharedOnExclusive) {
  Lock* lock = newLock();
  EXPECT_TRUE(getExclusiveAccess(lock, kTransactionIdA));
  EXPECT_FALSE(getSharedAccess(lock, kTransactionIdB));
};

// Cannot get exclusive access on an exclusive lock
TEST(LockTest, noExclusiveOnExclusive) {
  Lock* lock = newLock();
  EXPECT_TRUE(getExclusiveAccess(lock, kTransactionIdA));
  EXPECT_FALSE(getExclusiveAccess(lock, kTransactionIdB));
};

// Cannot acquire exclusive access on a shared lock
TEST(LockTest, noExclusiveOnShared) {
  Lock* lock = newLock();
  EXPECT_TRUE(getSharedAccess(lock, kTransactionIdA));
  EXPECT_FALSE(getExclusiveAccess(lock, kTransactionIdB));
};

// Upgrade
TEST(LockTest, upgrade) {
  Lock* lock = newLock();
  EXPECT_TRUE(getSharedAccess(lock, kTransactionIdA));
  EXPECT_TRUE(upgrade(lock, kTransactionIdA));
  EXPECT_TRUE(lock->exclusive);
  EXPECT_EQ(lock->num_owners, 1);
  EXPECT_EQ(lock->owners[0], kTransactionIdA);
}