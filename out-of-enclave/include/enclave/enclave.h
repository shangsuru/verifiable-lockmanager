#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <queue>
#include <string>
#include <vector>

#include "common.h"
#include "enclave_t.h"
#include "hashtable.h"
#include "integrity_verification.h"
#include "lock.h"
#include "lock_signatures.h"
#include "sgx_tcrypto.h"
#include "sgx_tkey_exchange.h"
#include "sgx_trts.h"
#include "transaction.h"

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
 * @param threadId the context for signing locks is exclusive for each thread,
 * therefore we need to know the calling thread's ID
 * @returns SGX_ERROR_UNEXPCTED, when transaction did not call
 * RegisterTransaction before or the given lock mode is unknown or when the
 * transaction makes a request for a look, that it already owns, makes a
 * request for a lock while in the shrinking phase, or when the lock budget is
 * exhausted
 */
auto acquire_lock(void *signature, int transactionId, int rowId,
                  bool isExclusive, int threadId) -> bool;

/**
 * Releases a lock for the specified row.
 *
 * @param transactionId identifies the transaction making the request
 * @param rowId identifies the row to be released
 */
void release_lock(int transactionId, int rowId);