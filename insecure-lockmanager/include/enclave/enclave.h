#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "enclave_t.h"
#include "lock.h"
#include "sgx_tcrypto.h"
#include "sgx_tkey_exchange.h"
#include "sgx_trts.h"
#include "sgx_tseal.h"
#include "transaction.h"

// Holds the transaction objects of the currently active transactions
std::unordered_map<unsigned int, Transaction *> transactionTable_;
size_t transactionTableSize_;

// Keeps track of a lock object for each row ID
std::unordered_map<unsigned int, Lock *> lockTable_;
size_t lockTableSize_;

// Contains configuration parameters
extern Arg arg_enclave;

/**
 * Transfers the configuration parameters from the untrusted application into
 * the enclave. Also initializes the job queue and associated mutexes and
 * condition variables.
 *
 * @param Arg configuration parameters
 */
void init_values(Arg arg);

/**
 * Function that receives a job from the untrusted application.
 * The job can be for example a lock request or a request to register a
 * transaction. The job is then put into a job queue for the responsible worker
 * thread.
 *
 * @param data struct that contains all arguments for the enclave to execute
 * the job, will be casted to (Job*) struct
 */
void send_job(void *data);

/**
 * Function that is run by the worker threads inside the enclave. It pulls a job
 * from its associated job queue in a loop and executes it, e.g. acquiring a
 * shared lock for a specific row. Each row gets assigned a specific thread
 * evenly, so no synchronization is necessary when accessing the underlying lock
 * table. The transaction table is accessed by only one single thread for all
 * requests to register a transaction.
 */
void worker_thread();

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
int acquire_lock(unsigned int transactionId, unsigned int rowId,
                 bool isExclusive);

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