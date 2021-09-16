#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "common.h"
#include "enclave.h"
#include "sgx_eid.h"
#include "sgx_tcrypto.h"
#include "sgx_urts.h"
#include "spdlog/spdlog.h"

/**
 * Process lock and unlock requests from the server. It manages a lock table,
 * where for each row ID it can store the corresponding lock object, which
 * contains information like type of lock and number of owners. It also manages
 * a transaction table, which maps from a transaction Id to the corresponding
 * transaction object, which holds information like the row IDs the transaction
 * has a lock on.
 *
 * It makes use of Intel SGX to have a secure enclave for signing the locks and
 * protecting its internal data structures.
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
   * acquire any locks.
   *
   * @param transactionId identifies the transaction
   */
  void registerTransaction(unsigned int transactionId);

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
   * transaction makes a request for a look, that it already owns or makes a
   * request for a lock while in the shrinking phase.
   */
  auto lock(unsigned int transactionId, unsigned int rowId, bool isExclusive)
      -> bool;

  /**
   * Releases a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   */
  void unlock(unsigned int transactionId, unsigned int rowId);

 private:
  /**
   * Function that each worker thread executes. It calls inside the enclave and
   * deals with incoming job requests.
   *
   * @param tmp not used
   */
  static auto load_and_initialize_threads(void *tmp) -> void *;

  /**
   * Initializes the configuration parameters for the enclave
   */
  void configuration_init();

  auto create_job(Command command, unsigned int transaction_id = 0,
                  unsigned int row_id = 0) -> bool;

  Arg arg;  // configuration parameters for the enclave
  pthread_t
      *threads;  // worker threads that execute requests inside the enclave
};