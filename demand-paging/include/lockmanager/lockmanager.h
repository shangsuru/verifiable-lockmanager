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
   * @returns false, if the transaction was already registered, else true
   */
  auto registerTransaction(unsigned int transactionId, unsigned int lockBudget)
      -> bool;

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
  auto lock(unsigned int transactionId, unsigned int rowId, bool isExclusive)
      -> std::pair<std::string, bool>;

  /**
   * Releases a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   */
  void unlock(unsigned int transactionId, unsigned int rowId);

 private:
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
  static auto load_and_initialize_threads(void *tmp) -> void *;

  /**
   * Initializes the configuration parameters for the enclave
   */
  void configuration_init();

  auto create_job(Command command, unsigned int transaction_id = 0,
                  unsigned int row_id = 0, unsigned int lock_budget = 0)
      -> std::pair<std::string, bool>;

  Arg arg;  // configuration parameters for the enclave
  pthread_t
      *threads;  // worker threads that execute requests inside the enclave
};