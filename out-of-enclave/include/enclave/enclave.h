#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <queue>
#include <string>
#include <vector>

#include "base64-encoding.h"
#include "common.h"
#include "enclave_t.h"
#include "hashtable.h"
#include "lock.h"
#include "sgx_tcrypto.h"
#include "sgx_tkey_exchange.h"
#include "sgx_trts.h"
#include "sgx_tseal.h"
#include "transaction.h"

// Context and public private key pair for signing lock requests
sgx_ecc_state_handle_t context = NULL;
sgx_ec256_private_t ec256_private_key;
sgx_ec256_public_t ec256_public_key;

const size_t MAX_SIGNATURE_LENGTH = 255;

// Base64 encoded public key
std::string encoded_public_key;

// Struct that gets sealed to storage to persist ECDSA keys
struct DataToSeal {
  sgx_ec256_private_t privateKey;
  sgx_ec256_public_t publicKey;
};

// Holds the transaction objects of the currently active transactions
HashTable *transactionTable_;

// Keeps track of a lock object for each row ID
HashTable *lockTable_;

/* Contains a list of hashes over the buckets of the table, which is used to
 * verify the integrity of the lock table. If the hash is recomputed and has
 * changed, it means the contents of the bucket changed.*/
std::vector<sgx_sha256_hash_t *> transactionTableIntegrityHashes;
std::vector<sgx_sha256_hash_t *> lockTableIntegrityHashes;

// Contains configuration parameters
extern Arg arg_enclave;

extern int transactionCount;

/**
 * Transfers the configuration parameters from the untrusted application into
 * the enclave. Also initializes the job queue and associated mutexes and
 * condition variables and the arrays holding the integrity hashes.
 *
 * @param arg configuration parameters
 * @param lock_table pointer to lock table whose memory was allocated in the
 * untrusted part
 * @param transaction_table pointer to transaction table whose memory was
 * allocated in the untrusted part
 */
void enclave_init_values(Arg arg, HashTable *lock_table,
                         HashTable *transaction_table);

/**
 * Function that receives a job from the untrusted application.
 * The job can be for example a lock request or a request to register a
 * transaction. The job is then put into a job queue for the responsible worker
 * thread.
 *
 * @param data struct that contains all arguments for the enclave to execute
 * the job, will be casted to (Job*) struct
 */
void enclave_send_job(void *data);

/**
 * Function that is run by the worker threads inside the enclave. It pulls a job
 * from its associated job queue in a loop and executes it, e.g. acquiring a
 * shared lock for a specific row. Each row gets assigned a specific thread
 * evenly, so no synchronization is necessary when accessing the underlying lock
 * table. The transaction table is accessed by only one single thread for all
 * requests to register a transaction.
 */
void enclave_process_request();

/**
 * @returns the size of the encrypted DataToSeal struct
 */
auto get_sealed_data_size() -> uint32_t;

/**
 * Seals the public and private key and stores it inside the sealed blob.
 * @param sealed_blob buffer to store sealed keys
 * @param sealed_size size of the sealed blob buffer
 * @returns SGX_SUCCESS or error code
 */
auto seal_keys(uint8_t *sealed_blob, uint32_t sealed_size) -> sgx_status_t;

/**
 * Unseals the keys stored in sealed_blob and sets global private and
 * public key attribute.
 * @param sealed_blob buffer that contains the sealed public and private keys
 * @param sealed_size size of the sealed blob buffer
 * @returns SGX_SUCCESS or error code
 */
auto unseal_keys(const uint8_t *sealed_blob, size_t data_size) -> sgx_status_t;

/**
 * Generates keys for ECDSA signature and sets corresponding private and
 * public key attribute.
 * @returns SGX_SUCCESS or error code
 */
auto generate_key_pair() -> int;

/**
 * Signs the message and stores the signature in given buffer.
 * @param message message to sign
 * @param signature buffer to store the signature
 * @param sig_len size of signature
 * @returns SGX_SUCCESS or error code
 */
auto sign(const char *message, void *signature, size_t sig_len) -> int;

/**
 * Verifies given message signature pair.
 * @param message message to verify
 * @param signature signature of message
 * @param sig_len size of signature
 * @returns SGX_SUCCESS or error code
 */
auto verify(const char *message, void *signature, size_t sig_len) -> int;

/**
 * Closes the ECDSA context.
 * @returns SGX_SUCCESS or error code
 */
auto ecdsa_close() -> int;

/**
 * Registers the transaction at the enclave prior to being able to
 * acquire any locks, so that the enclave can now the transaction's lock
 * budget.
 *
 * @param transactionId identifies the transaction
 * @param lockBudget maximum number of locks the transaction is allowed to
 * acquire
 */
int register_transaction(unsigned int transactionId, unsigned int lockBudget);

/**
 * Acquires a lock for the specified row and writes the signature into the
 * provided buffer.
 *
 * @param signature buffer where the enclave will store the signature
 * @param sig_len length of the buffer
 * @param transactionId identifies the transaction making the request
 * @param rowId identifies the row to be locked
 * @param requestedMode either shared for concurrent read access or exclusive
 * for sole write access
 * @returns SGX_ERROR_UNEXPCTED, when transaction did not call
 * RegisterTransaction before or the given lock mode is unknown or when the
 * transaction makes a request for a look, that it already owns, makes a
 * request for a lock while in the shrinking phase, or when the lock budget is
 * exhausted
 */
auto acquire_lock(void *signature, int transactionId, int rowId,
                  bool isExclusive) -> bool;

/**
 * Releases a lock for the specified row.
 *
 * @param transactionId identifies the transaction making the request
 * @param rowId identifies the row to be released
 */
void release_lock(int transactionId, int rowId);

/**
 * Releases all locks the given transaction currently has.
 *
 * @param transaction the transaction to be aborted
 */
void abort_transaction(Transaction *transaction);

/**
 * @returns the block timeout, which resembles a future block number of the
 *          blockchain in the storage layer. The storage layer will decline
 *          any requests with a signature that has a block timeout number
 *          smaller than the current block number
 */
auto get_block_timeout() -> int;

/**
 * This function is just for testing, to demonstrate that signatures created on
 * lock requests are valid.
 *
 * @param signature containing the signature for the lock that was
 * requested
 * @param transactionId identifying the transaction that requested the lock
 * @param rowId identifying the row the lock is refering to
 * @param isExclusive if the lock is a shared or exclusive lock (boolean)
 * @returns SGX_SUCCESS, when the signature is valid
 */
auto verify_signature(char *signature, int transactionId, int rowId,
                      int isExclusive) -> int;

/**
 *  Get string representation of the lock tuple:
 * <TRANSACTION-ID>_<ROW-ID>_<MODE>_<BLOCKTIMEOUT>, where mode means, if the
 * lock is for shared or exclusive access.
 *
 * @param transactionId identifies the transaction
 * @param rowId identifies the row that is locked
 * @param isExclusive if the lock is exclusive or shared
 * @returns a string that represents a lock, that can be signed by the signing
 * function
 */
auto lock_to_string(int transactionId, int rowId, bool isExclusive)
    -> std::string;

/**
 * Hashes a bucket of the lock table. The hash is saved by the enclave and can
 * be used to detect if the contents of the bucket was altered, by computing
 * the hash again and comparing it with the saved hash. If
 * they don't match, then something inside the bucket changed. The hash does not
 * include locks without owners.
 *
 * @param bucket the bucket to compute the hash over
 * @returns the hash over the given bucket
 */
auto hash_locktable_bucket(Entry *bucket) -> sgx_sha256_hash_t *;

/**
 * Hashes a bucket of the transaction table. The hash is saved by the enclave
 * and can be used to detect if the contents of the bucket was altered, by
 * computing the hash again and comparing it with the saved hash. If they don't
 * match, then something inside the bucket changed. The hash does not include
 * unregistered transactions, i.e. transactions with transaction_id = 0.
 *
 * @param bucket the bucket to compute the hash over
 * @returns the hash over the given bucket
 */
auto hash_transactiontable_bucket(Entry *bucket) -> sgx_sha256_hash_t *;

/**
 * Gets the entry from the lock table, but verifies the integrity of the
 * corresponding bucket via integrity hashes.
 *
 * @param integrityHashes hash over each bucket of the hashtable
 * @param hashTable the lock table
 * @param key the row ID
 * @returns the lock for the corresponding key, or
 * nullptr, when the verification of the hashes failed
 */
auto integrity_verified_get_locktable(int key) -> Lock *;

/**
 * Verifies the integrity hashes on a copy of the bucket in protected
 * memory, so that no changes can be made from untrusted memory while computing
 * the hash. Returns the bucket together with the corresponding transaction
 * within it. The returned transaction is therefore guaranteed to be an
 * unaltered copy from untrusted memory. Changes can be applied on it to update
 * the verification hash afterwards, but the changes need to be redone on the
 * corresponding transaction in untrusted memory. The returned transaction in
 * protected memory is just a temporary copy.
 *
 * @param key the transaction ID
 * @returns the transaction for the corresponding key together with the bucket
 * allocated in protected memory that was used to compute the integrity hash, or
 * nullptr, when the verification of the hashes failed. The bucket is needed to
 * recompute the integrity hash on a trusted copy when the transaction is
 * changed later, e.g., when registering or aborting it.
 */
auto integrity_verified_get_transactiontable(int key)
    -> std::pair<Transaction *, Entry *>;

/**
 * Copies the whole bucket from untrusted to protected memory. The entries are
 * assumed to contain locks.
 *
 * @param entry entry from untrusted memory to copy
 * @returns copy in protected memory
 */
auto copy_lock_bucket(Entry *entry) -> Entry *;

/**
 * Copies the whole bucket from untrusted to protected memory. The entries are
 * assumed to contain transactions.
 *
 * @param entry entry from untrusted memory to copy
 * @returns copy in protected memory
 */
auto copy_transaction_bucket(Entry *entry) -> Entry *;

/**
 * Computes the hash over the given bucket and updates the integrity hashes
 * stored inside the en
 */
void update_integrity_hash_transactiontable(Entry *entry);
