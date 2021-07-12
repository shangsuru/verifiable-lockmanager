#include <lockmanager.h>

void LockManager::registerTransaction(unsigned int transactionId,
                                      unsigned int lockBudget) {
  if (transactionTable_.contains(transactionId)) {
    throw std::domain_error("Transaction already registered");
  }

  transactionTable_.insert(
      transactionId, std::make_shared<Transaction>(transactionId, lockBudget));
};

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       Lock::LockMode requestedMode) -> std::string {
  // Get the transaction object for the given transaction ID
  std::shared_ptr<Transaction> transaction;
  try {
    transaction = transactionTable_.find(transactionId);
  } catch (const std::out_of_range& e) {
    throw std::domain_error("Transaction was not registered");
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
      lock->upgrade(transactionId);
      goto sign;
    }

    // Acquire lock in requested mode (shared, exclusive)
    if (!transaction->hasLock(rowId)) {
      transaction->addLock(rowId, requestedMode, lock);
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
  // Get the transaction object
  std::shared_ptr<Transaction> transaction;
  try {
    transaction = transactionTable_.find(transactionId);
  } catch (const std::out_of_range& e) {
    throw std::domain_error("Transaction was not registered");
  }

  // Get the lock object
  std::shared_ptr<Lock> lock;
  try {
    lock = lockTable_.find(rowId);
  } catch (const std::out_of_range& e) {
    throw std::domain_error("Lock does not exist");
  }

  transaction->releaseLock(rowId, lock);
};

auto LockManager::sign(unsigned int transactionId, unsigned int rowId,
                       unsigned int blockTimeout) const -> std::string {
  // TODO Implement signing the lock
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;

  std::string mode_indicator;
  switch (lockTable_.find(rowId)->getMode()) {
    case Lock::LockMode::kExclusive:
      mode_indicator = "X";
      break;
    case Lock::LockMode::kShared:
      mode_indicator = "S";
      break;
  };

  return std::to_string(transactionId) + "-" + std::to_string(rowId) +
         mode_indicator + "-" + std::to_string(blockTimeout);
};

void LockManager::abortTransaction(
    const std::shared_ptr<Transaction>& transaction) {
  transactionTable_.erase(transaction->getTransactionId());
  transaction->releaseAllLocks(lockTable_);
};

auto LockManager::getBlockTimeout() const -> unsigned int {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  // TODO Implement getting the block timeout from the blockchain
  std::cout << privateKey_ << std::endl;
  return 0;
};