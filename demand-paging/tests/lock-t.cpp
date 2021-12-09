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
  EXPECT_EQ(lock->owners_size, 4);
};

// Exclusive access works
TEST(LockTest, exclusiveAccess) {
  Lock* lock = newLock();
  getExclusiveAccess(lock, kTransactionIdA);
  EXPECT_TRUE(lock->exclusive);
  EXPECT_EQ(lock->owners_size, 1);

  bool containsId = false;
  for (int i = 0; i < lock->owners_size; i++) {
    if (lock->owners[i] == kTransactionIdA) {
      containsId = true;
    }
  }

  EXPECT_TRUE(containsId);
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
  EXPECT_EQ(lock->owners_size, 1);

  bool containsId = false;
  for (int i = 0; i < lock->owners_size; i++) {
    if (lock->owners[i] == kTransactionIdA) {
      containsId = true;
    }
  }

  EXPECT_TRUE(containsId);
}

TEST(LockTest, releaseUnownedLock) {
  Lock* lock = newLock();
  EXPECT_TRUE(getExclusiveAccess(lock, kTransactionIdA));
  // B tries to release the lock of A
  release(lock, kTransactionIdB);
  // This has no effect, A still owns the lock
  EXPECT_TRUE(lock->exclusive);
  EXPECT_EQ(lock->owners_size, 1);

  bool containsId = false;
  for (int i = 0; i < lock->owners_size; i++) {
    if (lock->owners[i] == kTransactionIdA) {
      containsId = true;
    }
  }

  EXPECT_TRUE(containsId);
}