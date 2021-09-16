#pragma once

#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "lock.h"
#include "spdlog/spdlog.h"
#include "transaction.h"

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
   * @param object pointer to lockmanager object (this) to be able to call
   * worker thread member function from within static function
   */
  static auto load_and_initialize_threads(void *tmp) -> void *;

  /**
   * Initializes the configuration parameters for the enclave
   */
  void configuration_init();

  /**
   * Sends a job to the job queue.
   *
   * @param command command for the job (SHARED, EXCLUSIVE, UNLOCK, REGISTER,
   * QUIT)
   * @param transaction_id optional parameter for SHARED, EXCLUSIVE, UNLOCK or
   * REGISTER
   * @param row_id optional parameter for SHARED, EXCLUSIVE or UNLOCK
   */
  auto create_job(Command command, unsigned int transaction_id = 0,
                  unsigned int row_id = 0) -> bool;

  /**
   * Function that is run by the worker threads inside the enclave. It pulls a
   * job from its associated job queue in a loop and executes it, e.g. acquiring
   * a shared lock for a specific row. Each row gets assigned a specific thread
   * evenly, so no synchronization is necessary when accessing the underlying
   * lock table. The transaction table is accessed by only one single thread for
   * all requests to register a transaction.
   */
  void worker_thread();

  /**
   * Function that receives a job from the untrusted application.
   * The job can be for example a lock request or a request to register a
   * transaction. The job is then put into a job queue for the responsible
   * worker thread.
   *
   * @param data struct that contains all arguments for the enclave to execute
   * the job, will be casted to (Job*) struct
   */
  void send_job(void *data);

  /**
   * Registers the transaction at the enclave prior to being able to
   * acquire any locks.
   *
   * @param transactionId identifies the transaction
   */
  int register_transaction(unsigned int transactionId);

  /**
   * Acquires a lock for the specified row.
   *
   * @param sig_len length of the buffer
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param requestedMode either shared for concurrent read access or exclusive
   * for sole write access
   * @returns SGX_ERROR_UNEXPCTED, when transaction did not call
   * RegisterTransaction before or the given lock mode is unknown or when the
   * transaction makes a request for a look, that it already owns, makes a
   * request for a lock while in the shrinking phase.
   */
  auto acquire_lock(unsigned int transactionId, unsigned int rowId,
                    bool isExclusive) -> bool;

  /**
   * Releases a lock for the specified row.
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   */
  void release_lock(unsigned int transactionId, unsigned int rowId);

  /**
   * Releases all locks the given transaction currently has.
   *
   * @param transaction the transaction to be aborted
   */
  void abort_transaction(Transaction *transaction);

  Arg arg;  // configuration parameters for the enclave
  pthread_t
      *threads;  // worker threads that execute requests inside the enclave
};