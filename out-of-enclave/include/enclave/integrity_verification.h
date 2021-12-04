#pragma once

#include <vector>

#include "common.h"
#include "enclave_t.h"
#include "lock.h"
#include "sgx_tcrypto.h"
#include "sgx_trts.h"
#include "transaction.h"

extern int sizeOfSerializedLockEntry;

/**
 * Computes the hash over the given bucket from the lock table and updates the
 * integrity hashes stored inside the enclave.
 *
 * @param bucket
 * @param key
 * @param numEntries
 * @param lockTableIntegrityHashes
 */
void update_integrity_hash_locktable(
    uint32_t *bucket, int key, int numEntries,
    std::vector<sgx_sha256_hash_t *> &lockTableIntegrityHashes);

/**
 * Hashes a bucket of the lock table. The hash is saved by the enclave
 * and can be used to detect if the contents of the bucket was altered, by
 * computing the hash again and comparing it with the saved hash. If they don't
 * match, then something inside the bucket changed.
 *
 * @param bucket the bucket to compute the hash over
 * @returns the hash over the given bucket
 */
auto hash_transactiontable_bucket(Entry *bucket) -> sgx_sha256_hash_t *;

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
auto hash_locktable_bucket(uint32_t *bucket, int numEntries)
    -> sgx_sha256_hash_t *;

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
 * @param transactionTable_ the transaction table to get the transaction from
 * @param transactionTableIntegrityHashes hash over each bucket of the
 * transaction table
 * @param key the transaction ID
 * @returns the transaction for the corresponding key together with the bucket
 * allocated in protected memory that was used to compute the integrity hash, or
 * nullptr, when the verification of the hashes failed. The bucket is needed to
 * recompute the integrity hash on a trusted copy when the transaction is
 * changed later, e.g., when registering or aborting it.
 */
auto integrity_verified_get_transactiontable(
    HashTable *&transactionTable_,
    std::vector<sgx_sha256_hash_t *> transactionTableIntegrityHashes, int key)
    -> std::pair<Transaction *, Entry *>;

/**
 * Serializes an entire bucket of the lock table into an uint32_t array that is
 * memory efficient and can be directly passed as a parameter to Intel SGX's
 * hash function for integrity verification.
 *
 * @param bucket a pointer to the first entry of the bucket
 * @param numEntries how many entries are in the given bucket
 * @returns the serialized bucket
 */
auto locktable_bucket_to_uint32_t(Entry *&bucket, int numEntries) -> uint32_t *;

/**
 * Adds a lock in the serialized bucket
 * @param transaction the (trusted) transaction that wants to acquire the lock
 * @param rowId the rowId of the lock to acquire
 * @param isExclusive if the lock should be exclusive or shared
 * @param serializedLockBucket
 * @returns true if the lock was acquired successfully, or false if the lock
 * couldn't get acquired, e.g. because it is already exclusive or integrity
 * verification failed.
 */
auto add_lock_trusted(Transaction *transaction, int rowId, bool isExclusive,
                      uint32_t *serializedLockBucket) -> bool;

/**
 * Removes a lock in the serialized bucket
 * @param transaction the (trusted) transaction that wants to release the lock
 * @param rowId the rowId of the lock to release
 * @param bucket the serialized bucket
 * @param numEntries how many entries the serialized bucket has
 * @returns true if the lock was acquired successfully, or false if the lock
 * couldn't get acquired, e.g. because it is already exclusive or integrity
 * verification failed.
 */
auto release_lock_trusted(Transaction *transaction, int rowId, uint32_t *bucket,
                          int numEntries) -> bool;

/**
 * Checks if the stored hash is the same as the hash of the given serialized
 * bucket
 *
 * @param serialized serialized form of the recently fetched lock table bucket
 * @param numEntries how many entries the given lock table bucket contains
 * @param storedHash the previously computed hash over the lock table bucket
 * @returns true if the integrity verification was successful, i.e., the hash
 * computed over the bucket in untrusted memory is the same as the one
 * previously computed and stored inside the enclave, meaning no change was done
 * to the lock table in untrusted memory
 */
auto verify_against_stored_hash(uint32_t *serialized, int numEntries,
                                sgx_sha256_hash_t *stored_hash) -> bool;