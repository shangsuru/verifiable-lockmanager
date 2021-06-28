#include <lockmanager.h>

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       Lock::LockMode requestedMode) -> std::string {
  // Get the transaction object for the given transaction ID
  std::shared_ptr<Transaction> transaction;
  try {
    transaction = transactionTable_.find(transactionId);
  } catch (const std::out_of_range& e) {
    throw std::invalid_argument("Transaction was not registered");
  }

  // Get the lock object for the given row ID
  std::shared_ptr<Lock> lock;
  try {
    lock = lockTable_.find(rowId);
  } catch (const std::out_of_range& e) {
    lock = std::make_shared<Lock>();
    lockTable_.insert(rowId, lock);
  }

  try {
    // Check if 2PL is violated
    if (transaction->getPhase() == Transaction::Phase::kShrinking) {
      throw std::domain_error("Cannot acquire more locks according to 2PL");
    }

    // Check if lock budget is enough
    if (transaction->getLockBudget() < 1) {
      throw std::domain_error("Lock budget exhausted");
    }

    // Check for upgrade request
    if (transaction->hasLock(rowId) &&
        requestedMode == Lock::LockMode::kExclusive &&
        lock->getMode() == Lock::LockMode::kShared) {
      lock->upgrade();
      goto sign;
    }

    // Check for exclusive request
    if (requestedMode == Lock::LockMode::kExclusive &&
        !transaction->hasLock(rowId)) {
      lock->getExclusiveAccess();
      transaction->addLock(rowId);
      goto sign;
    }

    // Check for shared request
    if (requestedMode == Lock::LockMode::kShared &&
        !transaction->hasLock(rowId)) {
      lock->getSharedAccess();
      transaction->addLock(rowId);
      goto sign;
    }

    throw std::domain_error("Request for already acquired lock");

  } catch (...) {
    abortTransaction(transaction);
    throw;
  }

sign:
  unsigned int block_timeout = getBlockTimeout();
  return sign(transactionId, rowId, block_timeout);
};

void LockManager::unlock(unsigned int transactionId, unsigned int rowId) {
  try {
    std::shared_ptr<Lock> lock = lockTable_.find(rowId);
    lock->release();
  } catch (const std::out_of_range& e) {
    std::cout << "The lock does not exist" << std::endl;
    return;
  }

  std::shared_ptr<Transaction> transaction;
  try {
    transaction = transactionTable_.find(transactionId);
  } catch (const std::out_of_range& e) {
    throw std::invalid_argument("Transaction was not registered");
  }

  transaction->deleteLock(rowId);
};

auto LockManager::sign(unsigned int transactionId, unsigned int rowId,
                       unsigned int blockTimeout) const -> std::string {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  std::cout << privateKey_;
  return "DUMMY_SIGNATURE";
};

auto LockManager::hasLock(const std::shared_ptr<Transaction>& transaction,
                          unsigned int rowId) -> bool {
  std::set<unsigned int> locked_rows = transaction->getLockedRows();
  return locked_rows.find(rowId) != locked_rows.end();
};

void LockManager::abortTransaction(
    const std::shared_ptr<Transaction>& transaction) {
  std::set<unsigned int> locked_rows = transaction->getLockedRows();
  for (auto locked_row : locked_rows) {
    lockTable_.find(locked_row)->release();
  }

  // TODO delete the transaction from the transaction table. Issues with
  // concurrent locking requests?
};

auto LockManager::getBlockTimeout() const -> unsigned int {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  // TODO Implement
  std::cout << privateKey_;
  return 0;
}