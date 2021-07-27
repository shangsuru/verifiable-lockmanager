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
  return 0;
};

auto LockManager::initialize_enclave() -> bool {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL,
                           &global_eid, NULL);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    return false;
  }
  return true;
}

auto LockManager::seal_and_save_keys() -> bool {
  uint32_t sealed_data_size = 0;
  sgx_status_t ret = get_sealed_data_size(global_eid, &sealed_data_size);

  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    return false;
  }

  if (sealed_data_size == UINT32_MAX) {
    return false;
  }

  uint8_t* temp_sealed_buf = (uint8_t*)malloc(sealed_data_size);
  if (temp_sealed_buf == NULL) {
    std::cerr << "Out of memory" << std::endl;
    return false;
  }

  sgx_status_t retval;
  ret = seal_keys(global_eid, &retval, temp_sealed_buf, sealed_data_size);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    free(temp_sealed_buf);
    return false;
  }

  if (retval != SGX_SUCCESS) {
    ret_error_support(retval);
    free(temp_sealed_buf);
    return false;
  }

  // Save the sealed blob
  if (!write_buf_to_file(SEALED_KEY_FILE, temp_sealed_buf, sealed_data_size,
                         0)) {
    std::cerr << "Failed to save the sealed data blob to \"" << SEALED_KEY_FILE
              << "\"" << std::endl;
    free(temp_sealed_buf);
    return false;
  }

  free(temp_sealed_buf);
  return true;
}

auto LockManager::read_and_unseal_keys() -> bool {
  sgx_status_t ret;
  // Read the sealed blob from the file
  size_t fsize = get_file_size(SEALED_KEY_FILE);
  if (fsize == (size_t)-1) {
    return false;
  }
  uint8_t* temp_buf = (uint8_t*)malloc(fsize);
  if (temp_buf == NULL) {
    std::cerr << "Out of memory" << std::endl;
    return false;
  }
  if (!read_file_to_buf(SEALED_KEY_FILE, temp_buf, fsize)) {
    free(temp_buf);
    return false;
  }

  // Unseal the sealed blob
  sgx_status_t retval;
  ret = unseal_keys(global_eid, &retval, temp_buf, fsize);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    free(temp_buf);
    return false;
  }

  if (retval != SGX_SUCCESS) {
    ret_error_support(retval);
    free(temp_buf);
    return false;
  }

  free(temp_buf);
  return true;
}