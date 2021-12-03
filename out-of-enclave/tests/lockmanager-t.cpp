#include <gtest/gtest.h>

#include "lock.h"
#include "lockmanager.h"

class LockManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { spdlog::set_level(spdlog::level::off); };

  const unsigned int kTransactionIdA = 1;
  const unsigned int kTransactionIdB = 2;
  const unsigned int kTransactionIdC = 3;
  const unsigned int kLockBudget = 100;
  const unsigned int kTransactionBudget = 2;
  const unsigned int kRowId = 1;
};

// Lock request aborts, when transaction is not registered
TEST_F(LockManagerTest, lockRequestAbortsWhenTransactionNotRegistered) {
  LockManager lock_manager = LockManager();
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
};

// Registering an already registered transaction
TEST_F(LockManagerTest, cannotRegisterTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_FALSE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
};

// Acquiring non-conflicting shared and exclusive locks works
TEST_F(LockManagerTest, acquiringLocks) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, true).second);
};

// Cannot get exclusive access when someone already has shared access
// TODO: Doesn't check if transaction already owns lock yet
TEST_F(LockManagerTest, DISABLED_wantExclusiveButAlreadyShared) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
};

// Cannot get shared access, when someone has exclusive access
// TODO: Doesn't check if transaction already owns lock yet
TEST_F(LockManagerTest, DISABLED_wantSharedButAlreadyExclusive) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, false).second);
};

// Several transactions can acquire a shared lock on the same row
TEST_F(LockManagerTest, multipleTransactionsSharedLock) {
  LockManager lock_manager = LockManager();
  for (unsigned int transaction_id = 1; transaction_id < kTransactionBudget;
       transaction_id++) {
    EXPECT_TRUE(lock_manager.registerTransaction(transaction_id, kLockBudget));
    EXPECT_TRUE(lock_manager.lock(transaction_id, kRowId, false).second);
  }
};

// Cannot get the same lock twice
// TODO: Doesn't check if transaction already owns lock yet
TEST_F(LockManagerTest, DISABLED_sameLockTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
};

// Lock budget runs out
TEST_F(LockManagerTest, lockBudgetRunsOut) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));

  unsigned int row_id = 1;
  for (; row_id <= kLockBudget; row_id++) {
    EXPECT_TRUE(lock_manager.lock(kTransactionIdA, row_id, false).second);
  }

  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, row_id, false).second);
};

// Can upgrade a lock
TEST_F(LockManagerTest, upgradeLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
};

// Can unlock and acquire again
TEST_F(LockManagerTest, unlock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  std::this_thread::sleep_for(
      std::chrono::seconds(1));  // unlock is asynchronous
  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
};

// Cannot request more locks after transaction aborted
// TODO: Abort not implemented
TEST_F(LockManagerTest, DISABLED_noMoreLocksAfterAbort) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, false).second);

  // Make transaction abort by acquiring the same lock again
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId + 1, false).second);

  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId + 2, false).second);
};

// Releasing a lock twice for the same transaction has no effect
TEST_F(LockManagerTest, releaseLockTwice) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  lock_manager.unlock(kTransactionIdA, kRowId);
};

TEST_F(LockManagerTest, releasingAnUnownedLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);

  // Transaction B tries to unlock A's lock and acquire it
  lock_manager.unlock(kTransactionIdB, kRowId);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
}

// Cannot acquire more locks in shrinking phase
TEST_F(LockManagerTest, noMoreLocksInShrinkingPhase) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, true).second);
};

TEST_F(LockManagerTest, verifySignature) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  std::string signature =
      lock_manager.lock(kTransactionIdA, kRowId, true).first;
  EXPECT_TRUE(lock_manager.verify_signature_string(signature, kTransactionIdA,
                                                   kRowId, true));
}

// TODO: Abort not implemented
TEST_F(LockManagerTest, DISABLED_abortedTransactionCanRegisterAgain) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId, true).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, kRowId, true)
                   .second);  // makes A abort
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
}

TEST_F(LockManagerTest,
       registrationFailsAfterTransactionTableIsAlteredInUntrustedMemory) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));

  // Maliciously change the contents of transaction table
  auto transaction =
      (Transaction*)get(lock_manager.transactionTable, kTransactionIdA);
  transaction->lock_budget = 10000;

  // It is not detected when we access a different bucket from the one where the
  // change happened
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  // Fails because an unauthorized change to the transaction table in the same
  // bucket was detected
  int anotherTransactionSameBucket =
      kTransactionIdA + lock_manager.transactionTable->size;
  EXPECT_FALSE(lock_manager.registerTransaction(anotherTransactionSameBucket,
                                                kLockBudget));
}

TEST_F(LockManagerTest,
       acquiringLockFailsAfterLockTableIsAlteredInUntrustedMemory) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);

  // Alter lock table in untrusted memory
  auto lock = (Lock*)get(lock_manager.lockTable, kRowId);
  lock->exclusive = true;

  // Next lock request fails because change is detected
  int anotherLockId =
      kRowId + lock_manager.lockTable
                   ->size;  // make sure that lock will be in the same bucket
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, anotherLockId, false).second);
}

TEST_F(LockManagerTest,
       acquiringLockFailsAfterLockTableIsAlteredInUntrustedMemory2) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);

  // Alter lock table in untrusted memory
  auto lock = (Lock*)get(lock_manager.lockTable, kRowId);
  lock->num_owners = 6;

  // Next lock request fails because change is detected
  int anotherLockId =
      kRowId + lock_manager.lockTable
                   ->size;  // make sure that lock will be in the same bucket
  EXPECT_FALSE(lock_manager.lock(kTransactionIdA, anotherLockId, false).second);
}

// TODO: Abort not implemented here
TEST_F(LockManagerTest,
       DISABLED_integrityVerificationWorksEvenWhenTransactionAborts) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));

  // Make transaction A acquire an exclusive lock
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, true).second);

  // Let transaction B try to acquire the same lock, which is a conflict and
  // causes B to abort
  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId + 1, true).second);
  EXPECT_FALSE(lock_manager.lock(kTransactionIdB, kRowId, true).second);

  // A can continue making lock requests, even the one that B had an exclusive
  // lock on, because B aborted
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId + 1, false).second);

  // B needs to register again, but afterwards can make more lock requests
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdB, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdB, kRowId + 1, false).second);
}

// After the last lock is released, the transaction gets deleted and it can
// register again.
// TODO: Releasing untrusted memory does not work from enclave
// apparently (needs further testing).
TEST_F(LockManagerTest, DISABLED_transactionGetsDeletedAfterReleasingLastLock) {
  LockManager lock_manager = LockManager();
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, kRowId, false).second);
  lock_manager.unlock(kTransactionIdA, kRowId);
  std::this_thread::sleep_for(std::chrono::seconds(
      1));  // need to wait here because unlock is asynchronous
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, kLockBudget));
}

TEST_F(LockManagerTest, notWaitingForSignature) {
  LockManager lock_manager = LockManager();
  int lockBudget = 100;
  EXPECT_TRUE(lock_manager.registerTransaction(kTransactionIdA, lockBudget));
  for (int i = 1; i < lockBudget; i++) {
    lock_manager.lock(kTransactionIdA, i, false,
                      +true);  // not waiting for signature return value
  }
  EXPECT_TRUE(lock_manager.lock(kTransactionIdA, lockBudget, false,
                                true)
                  .second);  // waitung for signature return value at the end
}