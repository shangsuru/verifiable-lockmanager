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
 */
class LockManager {
 public:
  /**
   * Initializes the job queue, mutexes and the configuration parameters.
   */
  LockManager();

  /**
   * Shuts down the worker threads.
   */
  virtual ~LockManager();

  /**
   * Registers the transaction at the lock manager prior to being able to
   * acquire any locks.
   *
   * @param transactionId identifies the transaction
   * @returns false, if the transaction was already registered, else true
   */
  auto registerTransaction(unsigned int transactionId) -> bool;

  /**
   * Acquires a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param requestedMode either shared for concurrent read access or exclusive
   * for sole write access
   * @returns if successful or not. E.g., when the transaction did not call
   * RegisterTransaction before or the given lock mode is unknown or when the
   * transaction makes a request for a look, that it already owns or makes a
   * request for a lock while in the shrinking phase, the request will fail.
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
   * Initializes configuration parameters.
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
   * Function that receives a job, which can be a lock request or a request to
   * register a transaction. The job is then put into a job queue for the
   * responsible worker thread.
   *
   * @param data struct that contains all arguments for the enclave to execute
   * the job, will be casted to (Job*) struct
   */
  void send_job(void *data);

  /**
   * Acquires a lock for the specified row.
   *
   * @param sig_len length of the buffer
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param isExclusive either shared for concurrent read access or exclusive
   * for sole write access
   * @returns false, when transaction did not call
   * RegisterTransaction before or the given lock mode is unknown or when the
   * transaction makes a request for a look, that it already owns, makes a
   * request for a lock while in the shrinking phase, else true
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

  pthread_mutex_t global_mutex;  // synchronizes access to num
  pthread_mutex_t *queue_mutex;  // synchronizes access to the job queue
  pthread_cond_t
      *job_cond;  // wakes up worker threads when a new job is available
  std::vector<std::queue<Job>> queue;  // a job queue for each worker thread

  // Holds the transaction objects of the currently active transactions
  std::unordered_map<unsigned int, Transaction *> transactionTable_;

  // Keeps track of a lock object for each row ID
  std::unordered_map<unsigned int, Lock *> lockTable_;
  int num = 0;  // global variable used to give every thread a unique ID
};