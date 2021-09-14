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
#define MAX_PATH FILENAME_MAX
#define NOT_SET '-'      // to fill empty signature buffer
#define NO_SIGNATURE ""  // for jobs that return no signature (QUIT, UNLOCK)
#define FAILED '+'       // signaling that a lock couldn't get acquired

static hashtable *ht = NULL;
static MACbuffer *MACbuf = NULL;

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

  auto load_and_initialize_enclave(sgx_enclave_id_t *eid) -> sgx_status_t;

  auto macbuffer_create(int size) -> MACbuffer *;

  auto ht_create(int size) -> hashtable *;

  void start_worker_threads();

  static auto load_and_initialize_threads(void *object) -> void *;

  void configuration_init();

  auto create_job(Command command, unsigned int transaction_id = 0,
                  unsigned int row_id = 0) -> std::pair<std::string, bool>;

  sgx_enclave_id_t global_eid = 0;
  sgx_launch_token_t token = {0};
  Arg arg;
  pthread_t *threads;
};