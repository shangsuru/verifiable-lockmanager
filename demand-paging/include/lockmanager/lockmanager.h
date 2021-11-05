#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "base64-encoding.h"
#include "common.h"
#include "enclave_u.h"
#include "errors.h"
#include "files.h"
#include "sgx_eid.h"
#include "sgx_tcrypto.h"
#include "sgx_urts.h"
#include "spdlog/spdlog.h"

#define TOKEN_FILENAME "enclave.token"
#define ENCLAVE_FILENAME "enclave.signed.so"
#define SEALED_KEY_FILE "sealed_data_blob.txt"
#define NO_SIGNATURE ""    // for jobs that return no signature (QUIT, UNLOCK)
#define SIGNATURE_SIZE 89  // length of the base64-encoded signature

extern sgx_enclave_id_t global_eid;  // identifies the enclave
extern sgx_launch_token_t token;

//=========================== OCALLS ============================
/**
 * Logs an info message from inside the enclave to the terminal
 *
 * @param str characters to be printed
 */
void print_info(const char *str);

/**
 * Logs an error message from inside the enclave to the terminal
 *
 * @param str characters to be printed
 */
void print_error(const char *str);

/**
 * Logs a warning message from inside the enclave to the terminal
 *
 * @param str characters to be printed
 */
void print_warn(const char *str);

/**
 * Logs a debug message from inside the enclave to the terminal
 *
 * @param str characters to be printed
 */
void print_debug(const char *str);
//================================================================

/**
 * Process lock and unlock requests from the server. It manages a lock table,
 * where for each row ID it can store the corresponding lock object, which
 * contains information like type of lock and number of owners. It also manages
 * a transaction table, which maps from a transaction Id to the corresponding
 * transaction object, which holds information like the row IDs the transaction
 * has a lock on or the transaction's lock budget.
 *
 * It makes use of Intel SGX to have a secure enclave for signing the locks and
 * protecting its internal data structures.
 */
class LockManager {
 public:
  /**
   * Initializes the enclave and seals the public and private key for signing.
   *
   * @param numWorkerThreads the number of threads that work on the lock table
   */
  LockManager(int numWorkerThreads = 1);

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
   * @returns false, if the transaction was already registered, else true
   */
  auto registerTransaction(unsigned int transactionId, unsigned int lockBudget)
      -> bool;

  /**
   * Acquires a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param isExclusive either shared for concurrent read access or exclusive
   * for sole write access
   * @param waitForResult parameter forwarded to create_job function
   * @returns the signature for the acquired lock and true or
   * no signature and false, when transaction was not registered before or when
   * the transaction makes a request for a look that it already owns, makes a
   * request for a lock while in the shrinking phase, or when the lock budget is
   * exhausted
   */
  auto lock(unsigned int transactionId, unsigned int rowId, bool isExclusive,
            bool waitForResult = true) -> std::pair<std::string, bool>;

  /**
   * Releases a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   * @param waitForResult if true makes unlock a synchronous operation, else
   * asynchronous (no waiting for operation to be finished)
   */
  void unlock(unsigned int transactionId, unsigned int rowId,
              bool waitForResult = false);

  /**
   * This function is just for testing, to demonstrate that signatures created
   * on lock requests are valid.
   *
   * @param signature containing the signature for the lock that was
   * requested
   * @param transactionId identifying the transaction that requested the lock
   * @param rowId identifying the row the lock is refering to
   * @param isExclusive if the lock is a shared or exclusive lock (boolean)
   * @returns true, when the signature is valid
   */
  auto verify_signature_string(std::string signature, int transactionId,
                               int rowId, int isExclusive) -> bool;

 private:
  /**
   * Stores key pair for ECDSA signature inside the sealed key file. This is
   * necessary to persist the key pair that the enclave uses for signing even if
   * it temporarily shuts down.
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

  /**
   * Starts the enclave.
   *
   * @param eid specifying the enclave
   * @returns success or failure
   */
  auto load_and_initialize_enclave(sgx_enclave_id_t *eid) -> sgx_status_t;

  /**
   * Function that each worker thread executes. It calls inside the enclave and
   * deals with incoming job requests.
   *
   * @param tmp not used
   */
  static auto create_worker_thread(void *tmp) -> void *;

  /**
   * Initializes the configuration parameters for the enclave
   *
   * @param numWorkerThreads the number of threads that work on the lock table
   */
  void configuration_init(int numWorkerThreads);

  /**
   * Creates a job and sends it to the enclave to get it processed by an enclave
   * worker thread.
   *
   * @param command SHARED, EXCLUSIVE, REGISTER or QUIT
   * @param transaction_id additional argument for SHARED, EXCLUSIVE or REGISTER
   * @param row_id additional argument for SHARED or EXCLUSIVE
   * @param lock_budget additional argument for REGISTER
   * @param waitForResult if the function should wait for return values to be
   * set or immediately return
   * @returns a pair containing a boolean, that is true when the job was
   * executed successfully and if true and the command was for a lock request,
   * the pair also contains the signature as the return value
   */
  auto create_enclave_job(Command command, unsigned int transaction_id = 0,
                          unsigned int row_id = 0, unsigned int lock_budget = 0,
                          bool waitForResult = true)
      -> std::pair<std::string, bool>;

  Arg arg;  // configuration parameters for the enclave
  pthread_t
      *threads;  // worker threads that execute requests inside the enclave
};