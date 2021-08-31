#include "enclave.h"

auto get_block_timeout() -> unsigned int {
  print_warn("Lock timeout not yet implemented");
  // TODO: Implement getting the lock timeout
  return 0;
};

auto get_sealed_data_size() -> uint32_t {
  return sgx_calc_sealed_data_size((uint32_t)encoded_public_key.length(),
                                   sizeof(DataToSeal{}));
}

auto seal_keys(uint8_t *sealed_blob, uint32_t sealed_size) -> sgx_status_t {
  print_info("Sealing keys");
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  sgx_sealed_data_t *sealed_data = NULL;
  DataToSeal data;
  data.privateKey = ec256_private_key;
  data.publicKey = ec256_public_key;

  if (sealed_size != 0) {
    sealed_data = (sgx_sealed_data_t *)malloc(sealed_size);
    ret = sgx_seal_data((uint32_t)encoded_public_key.length(),
                        (uint8_t *)encoded_public_key.c_str(), sizeof(data),
                        (uint8_t *)&data, sealed_size, sealed_data);
    if (ret == SGX_SUCCESS)
      memcpy(sealed_blob, sealed_data, sealed_size);
    else
      free(sealed_data);
  }
  return ret;
}

auto unseal_keys(const uint8_t *sealed_blob, size_t sealed_size)
    -> sgx_status_t {
  print_info("Unsealing keys");
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  DataToSeal *unsealed_data = NULL;

  uint32_t dec_size = sgx_get_encrypt_txt_len((sgx_sealed_data_t *)sealed_blob);
  uint32_t mac_text_len =
      sgx_get_add_mac_txt_len((sgx_sealed_data_t *)sealed_blob);

  uint8_t *mac_text = (uint8_t *)malloc(mac_text_len);
  if (dec_size != 0) {
    unsealed_data = (DataToSeal *)malloc(dec_size);
    sgx_sealed_data_t *tmp = (sgx_sealed_data_t *)malloc(sealed_size);
    memcpy(tmp, sealed_blob, sealed_size);
    ret = sgx_unseal_data(tmp, mac_text, &mac_text_len,
                          (uint8_t *)unsealed_data, &dec_size);
    if (ret != SGX_SUCCESS) goto error;
    ec256_private_key = unsealed_data->privateKey;
    ec256_public_key = unsealed_data->publicKey;
  }

error:
  if (unsealed_data != NULL) free(unsealed_data);
  return ret;
}

auto generate_key_pair() -> int {
  print_info("Creating new key pair");
  if (context == NULL) sgx_ecc256_open_context(&context);
  sgx_status_t ret = sgx_ecc256_create_key_pair(&ec256_private_key,
                                                &ec256_public_key, context);
  encoded_public_key = base64_encode((unsigned char *)&ec256_public_key,
                                     sizeof(ec256_public_key));

  // append the number of characters for the encoded public key for easy
  // extraction from sealed text file
  encoded_public_key += std::to_string(encoded_public_key.length());
  return ret;
}

// Verifies a given message with its signature object and returns on success
// SGX_EC_VALID or on failure SGX_EC_INVALID_SIGNATURE
auto verify(const char *message, void *signature, size_t sig_len) -> int {
  sgx_ecc256_open_context(&context);
  uint8_t res;
  sgx_ec256_signature_t *sig = (sgx_ec256_signature_t *)signature;
  sgx_status_t ret =
      sgx_ecdsa_verify((uint8_t *)message, strnlen(message, MAX_MESSAGE_LENGTH),
                       &ec256_public_key, sig, &res, context);
  return res;
}

// Closes the ECDSA context
auto ecdsa_close() -> int {
  if (context == NULL) sgx_ecc256_open_context(&context);
  return sgx_ecc256_close_context(context);
}

int register_transaction(unsigned int transactionId, unsigned int lockBudget) {
  print_info("Registering transaction");
  if (transactionTable_.contains(transactionId)) {
    print_error("Transaction is already registered");
    return SGX_ERROR_UNEXPECTED;
  }

  transactionTable_.set(
      transactionId, std::make_shared<Transaction>(transactionId, lockBudget));

  return SGX_SUCCESS;
}

int acquire_lock(void *signature, size_t sig_len, unsigned int transactionId,
                 unsigned int rowId, int isExclusive) {
  // Get the transaction object for the given transaction ID
  std::shared_ptr<Transaction> transaction =
      transactionTable_.get(transactionId);
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return SGX_ERROR_UNEXPECTED;
  }
  // Get the lock object for the given row ID
  std::shared_ptr<Lock> lock = lockTable_.get(rowId);
  if (lock == nullptr) {
    lock = std::make_shared<Lock>();
    lockTable_.set(rowId, lock);
  }

  // Check if 2PL is violated
  if (transaction->getPhase() == Transaction::Phase::kShrinking) {
    print_error("Cannot acquire more locks according to 2PL");
    goto abort;
  }

  // Check if lock budget is enough
  if (transaction->getLockBudget() < 1) {
    print_error("Lock budget exhausted");
    goto abort;
  }

  // Check for upgrade request
  if (transaction->hasLock(rowId) && isExclusive &&
      lock->getMode() == Lock::LockMode::kShared) {
    lock->upgrade(transactionId);
    goto sign;
  }

  // Acquire lock in requested mode (shared, exclusive)
  if (!transaction->hasLock(rowId)) {
    if (isExclusive) {
      transaction->addLock(rowId, Lock::LockMode::kExclusive, lock);
    } else {
      transaction->addLock(rowId, Lock::LockMode::kShared, lock);
    }
    goto sign;
  }

  print_error("Request for already acquired lock");

abort:
  abort_transaction(transaction);
  return SGX_ERROR_UNEXPECTED;

sign:
  unsigned int block_timeout = get_block_timeout();
  /* Get string representation of the lock tuple:
   * <TRANSACTION-ID>_<ROW-ID>_<MODE>_<BLOCKTIMEOUT>,
   * where mode means, if the lock is for shared or exclusive access
   */
  std::string mode;
  switch (lockTable_.get(rowId)->getMode()) {
    case Lock::LockMode::kExclusive:
      mode = "X";  // exclusive
      break;
    case Lock::LockMode::kShared:
      mode = "S";  // shared
      break;
  };

  std::string string_to_sign = std::to_string(transactionId) + "_" +
                               std::to_string(rowId) + "_" + mode + "_" +
                               std::to_string(block_timeout);

  sgx_ecc_state_handle_t context = NULL;
  sgx_ecc256_open_context(&context);
  sgx_ecdsa_sign((uint8_t *)string_to_sign.c_str(),
                 strnlen(string_to_sign.c_str(), MAX_MESSAGE_LENGTH),
                 &ec256_private_key, (sgx_ec256_signature_t *)signature,
                 context);

  int ret = verify(string_to_sign.c_str(), (void *)signature,
                   sizeof(sgx_ec256_signature_t));
  if (ret != SGX_SUCCESS) {
    print_error("Failed to verify signature");
  } else {
    print_info("Signature successfully verified");
  }

  return ret;
}

void release_lock(unsigned int transactionId, unsigned int rowId) {
  // Get the transaction object
  std::shared_ptr<Transaction> transaction;
  transaction = transactionTable_.get(transactionId);
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return;
  }

  // Get the lock object
  std::shared_ptr<Lock> lock;
  lock = lockTable_.get(rowId);
  if (lock == nullptr) {
    print_error("Lock does not exist");
    return;
  }

  transaction->releaseLock(rowId, lock);
}

void abort_transaction(const std::shared_ptr<Transaction> &transaction) {
  transactionTable_.erase(transaction->getTransactionId());
  transaction->releaseAllLocks(lockTable_);
}
