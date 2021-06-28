#include <lockmanager.h>

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       Lock::LockMode requestedMode) -> std::string {
  std::shared_ptr<Transaction> transaction;
  try {
    transaction = transactionTable_.find(transactionId);
  } catch (const std::out_of_range& e) {
    throw std::invalid_argument("Transaction was not registered");
  }

  if (!lockTable_.contains(rowId)) {
    lockTable_.insert(rowId, std::make_shared<Lock>());
  }
  std::shared_ptr<Lock> lock = lockTable_.find(rowId);

  try {
    if (transaction->getPhase() == Transaction::Phase::kShrinking) {
      throw std::domain_error("Cannot acquire more locks according to 2PL");
    }

    if (transaction->getLockBudget() < 1) {
      throw std::domain_error("Lock budget exhausted");
    }

    if (hasLock(transaction, rowId)) {
      if (requestedMode == Lock::LockMode::kExclusive &&
          lock->getMode() == Lock::LockMode::kShared) {
        lock->upgrade();
      } else {
        throw std::domain_error("Request for already acquired lock");
      }
    } else {
      if (requestedMode == Lock::LockMode::kExclusive) {
        lock->getExclusiveAccess();
        transaction->addLock(rowId);
      } else if (requestedMode == Lock::LockMode::kShared) {
        lock->getSharedAccess();
        transaction->addLock(rowId);
      } else {
        throw std::invalid_argument("Invalid lock mode");
      }
    }
  } catch (...) {
    abortTransaction(transaction);
    throw;
  }

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

auto getBlockTimeout() -> unsigned int {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  // TODO Implement
  return 0;
}