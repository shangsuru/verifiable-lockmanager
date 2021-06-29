#include <gtest/gtest.h>

#include "lock.h"
#include "lockmanager.h"

// Lock request aborts, when transaction is not registered
TEST(LockManagerTest, lockRequestAbortsWhenTransactionNotRegistered) {
  LockManager lock_manager = LockManager();

  try {
    lock_manager.lock(1, 2, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction was not registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Registering an already registered transaction
TEST(LockManagerTest, cannotRegisterTwice) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id = 1;
  const unsigned int lock_budget = 10;

  try {
    lock_manager.registerTransaction(transaction_id, lock_budget);
    lock_manager.registerTransaction(transaction_id, lock_budget);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction already registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Acquiring non-conflicting shared and exclusive locks works
TEST(LockManagerTest, acquiringLocks) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id = 1;
  const unsigned int lock_budget = 10;

  lock_manager.registerTransaction(transaction_id, lock_budget);
  std::string signature_s =
      lock_manager.lock(transaction_id, 1, Lock::LockMode::kShared);
  std::string signature_x =
      lock_manager.lock(transaction_id, 2, Lock::LockMode::kExclusive);

  EXPECT_EQ(signature_s, "1-1S-0");
  EXPECT_EQ(signature_x, "1-2X-0");
};

// Cannot get exclusive access when someone already has shared access
TEST(LockManagerTest, wantExclusiveButAlreadyShared) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id_a = 1;
  const unsigned int transaction_id_b = 2;
  const unsigned int row_id = 3;
  const unsigned int lock_budget = 10;

  lock_manager.registerTransaction(transaction_id_a, lock_budget);
  lock_manager.registerTransaction(transaction_id_b, lock_budget);

  lock_manager.lock(transaction_id_a, row_id, Lock::LockMode::kShared);
  try {
    lock_manager.lock(transaction_id_b, row_id, Lock::LockMode::kExclusive);
    FAIL() << "Expected std::domain error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain error";
  }
};

// Cannot get shared access, when someone has exclusive access
TEST(LockManagerTest, wantSharedButAlreadyExclusive) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id_a = 1;
  const unsigned int transaction_id_b = 2;
  const unsigned int row_id = 3;
  const unsigned int lock_budget = 10;

  lock_manager.registerTransaction(transaction_id_a, lock_budget);
  lock_manager.registerTransaction(transaction_id_b, lock_budget);

  lock_manager.lock(transaction_id_a, row_id, Lock::LockMode::kExclusive);
  try {
    lock_manager.lock(transaction_id_b, row_id, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Couldn't acquire lock"));
  } catch (...) {
    FAIL() << "Expected std::domain error";
  }
};

// Several transactions can acquire a shared lock on the same row
TEST(LockManagerTest, multipleTransactionsSharedLock) {
  LockManager lock_manager = LockManager();
  const unsigned int row_id = 3;
  const unsigned int lock_budget = 10;

  for (unsigned int transaction_id = 0; transaction_id < lock_budget;
       transaction_id++) {
    lock_manager.registerTransaction(transaction_id, lock_budget);
    std::string signature =
        lock_manager.lock(transaction_id, row_id, Lock::LockMode::kShared);
    EXPECT_EQ(signature, std::to_string(transaction_id) + "-3S-0");
  }
};

// Lock budget runs out
TEST(LockManagerTest, lockBudgetRunsOut) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id = 0;
  const unsigned int lock_budget = 10;

  lock_manager.registerTransaction(transaction_id, lock_budget);

  unsigned int row_id = 0;
  for (; row_id < lock_budget; row_id++) {
    std::string signature =
        lock_manager.lock(transaction_id, row_id, Lock::LockMode::kShared);
    std::string expected_signature = "0-" + std::to_string(row_id) + "S-0";
    EXPECT_EQ(signature, expected_signature);
  }

  try {
    lock_manager.lock(transaction_id, row_id + 1, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Lock budget exhausted"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Can upgrade a lock
TEST(LockManagerTest, upgradeLock) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id = 0;
  const unsigned int row_id = 2;
  const unsigned int lock_budget = 2;

  lock_manager.registerTransaction(transaction_id, lock_budget);
  std::string signature_a =
      lock_manager.lock(transaction_id, row_id, Lock::LockMode::kShared);
  std::string signature_b =
      lock_manager.lock(transaction_id, row_id, Lock::LockMode::kExclusive);

  EXPECT_EQ(signature_a, "0-2S-0");
  EXPECT_EQ(signature_b, "0-2X-0");
};

// Can unlock and acquire again
TEST(LockManagerTest, unlock) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id_a = 0;
  const unsigned int transaction_id_b = 1;
  const unsigned int transaction_id_c = 2;
  const unsigned int row_id = 2;
  const unsigned int lock_budget = 2;

  lock_manager.registerTransaction(transaction_id_a, lock_budget);
  lock_manager.registerTransaction(transaction_id_b, lock_budget);
  lock_manager.registerTransaction(transaction_id_c, lock_budget);

  std::string signature_a =
      lock_manager.lock(transaction_id_a, row_id, Lock::LockMode::kShared);
  std::string signature_b =
      lock_manager.lock(transaction_id_b, row_id, Lock::LockMode::kShared);

  EXPECT_EQ(signature_a, "0-2S-0");
  EXPECT_EQ(signature_b, "1-2S-0");

  lock_manager.unlock(transaction_id_a, row_id);
  lock_manager.unlock(transaction_id_b, row_id);

  std::string signature_c =
      lock_manager.lock(transaction_id_c, row_id, Lock::LockMode::kExclusive);

  EXPECT_EQ(signature_c, "2-2X-0");
};

// Cannot request more locks after transaction aborted
TEST(LockManagerTest, noMoreLocksAfterAbort) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id = 0;
  const unsigned int lock_budget = 5;

  lock_manager.registerTransaction(transaction_id, lock_budget);
  lock_manager.lock(transaction_id, 0, Lock::LockMode::kShared);
  lock_manager.lock(transaction_id, 1, Lock::LockMode::kShared);

  // Make transaction abort by acquiring the same lock again
  try {
    lock_manager.lock(transaction_id, 1, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request for already acquired lock"));
  }

  try {
    lock_manager.lock(transaction_id, 2, Lock::LockMode::kShared);
    FAIL() << "Expected std::domain_error";
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Transaction was not registered"));
  } catch (...) {
    FAIL() << "Expected std::domain_error";
  }
};

// Releasing a lock twice for the same transaction has no effect
TEST(LockManagerTest, releaseLockTwice) {
  LockManager lock_manager = LockManager();
  const unsigned int transaction_id = 0;
  const unsigned int lock_budget = 5;
  const unsigned int row_id = 3;

  lock_manager.registerTransaction(transaction_id, lock_budget);
  lock_manager.lock(transaction_id, row_id, Lock::LockMode::kExclusive);
  lock_manager.unlock(transaction_id, row_id);
  lock_manager.unlock(transaction_id, row_id);
};
