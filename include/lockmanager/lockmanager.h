#pragma once

#include <iostream>
#include <libcuckoo/cuckoohash_map.hh>
#include <memory>
#include <stdexcept>
#include <string>

#include "spdlog/spdlog.h"

#include "base64-encoding.h"
#include "enclave_u.h"
#include "errors.h"
#include "files.h"
#include "lock.h"
#include "sgx_eid.h"
#include "sgx_tcrypto.h"
#include "sgx_urts.h"
#include "transaction.h"

using libcuckoo::cuckoohash_map;

#define TOKEN_FILENAME "enclave.token"
#define ENCLAVE_FILENAME "enclave.signed.so"
#define SEALED_KEY_FILE "sealed_data_blob.txt"
#define MAX_PATH FILENAME_MAX

/**
 * Process lock and unlock requests from the server. It manages a lock table,
 * where for each row ID it can store the corresponding lock object, which
 * contains information like type of lock and number of owners. It also manages
 * a transaction table, which maps from a transaction Id to the corresponding
 * transaction object, which holds information like the row IDs the transaction
 * has a lock on or the transaction's lock budget.
 * 
 * It makes use of Intel SGX to have a secure enclave for signing the locks and protecting
 * its internal data structures.
 */
class LockManager {
 public:

  /**
   * Initializes the enclave and seals the public and private key for signing.
   */
  LockManager();

  /**
   * Destroys the enclave.
   */
  virtual ~LockManager();

  /**
   * Registers the transaction at the lock manager prior to being able to
   * acquire any locks, so that the lock manager can now the transaction's lock
   * budget.
   *
   * @param transactionId identifies the transaction
   * @param lockBudget maximum number of locks the transaction is allowed to
   * acquire
   */
  void registerTransaction(unsigned int transactionId, unsigned int lockBudget);

  /**
   * Acquires a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param requestedMode either shared for concurrent read access or exclusive
   * for sole write access
   * @returns the signature for the acquired lock
   * @throws std::domain_error, when transaction did not call
   * RegisterTransaction before or the given lock mode is unknown or when the
   * transaction makes a request for a look, that it already owns, makes a
   * request for a lock while in the shrinking phase, or when the lock budget is
   * exhausted
   */
  auto lock(unsigned int transactionId, unsigned int rowId,
            Lock::LockMode requestedMode) -> std::string;

  /**
   * Releases a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   * @throws std::domain_error, when the transaction didn't call
   * RegisterTransaction before or the lock to unlock does not exist
   */
  void unlock(unsigned int transactionId, unsigned int rowId);

 private:
  /**
   * Releases all locks the given transaction currently has.
   *
   * @param transaction the transaction to be aborted
   */
  void abortTransaction(const std::shared_ptr<Transaction>& transaction);

  /**
   * @returns the block timeout, which resembles a future block number of the
   *          blockchain in the storage layer. The storage layer will decline
   *          any requests with a signature that has a block timeout number
   *          smaller than the current block number.
   */
  auto getBlockTimeout() const -> unsigned int;

  /**
   * Signs a lock with the private key of the lock manager. The signature can be
   * returned to the client, so he can verify to the storage layer, that he
   * acquired the lock for the respective read or write access to a row.
   *
   * @param transactionId identifies the transaction holding the lock
   * @param rowId identifies the row the lock was acquired for
   * @param blockTimeout timeout counter that invalidates the signature after
   *                     the block number of the underlying blockchain in the
   *                     storage layer reaches the block timeout number
   * @returns the signature of the lock
   */
  auto getLockSignature(unsigned int transactionId, unsigned int rowId,
            unsigned int blockTimeout) const -> std::string;

  /**
   * Initializes the enclave (in DEBUG mode).
   *
   * @returns true if successful, else false
   */
  auto initialize_enclave() -> bool;

  /**
   * Stores key pair for ECDSA signature inside the sealed key file.
   *
   * @returns true if successful, else false
   */
  auto seal_and_save_keys() -> bool;

  /**
   * Unseals key pair for ECDSA signature from the sealed key file
   * and sets them as the current public and private key.
   *
   * @returns true, if successful, else false
   */
  auto read_and_unseal_keys() -> bool;

  cuckoohash_map<unsigned int, std::shared_ptr<Lock>> lockTable_;
  cuckoohash_map<unsigned int, std::shared_ptr<Transaction>> transactionTable_;
  sgx_enclave_id_t global_eid = 0;
};