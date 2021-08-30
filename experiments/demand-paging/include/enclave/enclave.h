#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <string>

#include "base64-encoding.h"
#include "enclave_t.h"
#include "hashtable.h"
#include "lock.h"
#include "sgx_tcrypto.h"
#include "sgx_tseal.h"
#include "transaction.h"

sgx_ecc_state_handle_t context = NULL;
sgx_ec256_private_t ec256_private_key;
sgx_ec256_public_t ec256_public_key;
/* Base64 encoded ec256_public_key with char count appended at the end
 * for easy extraction from sealed data file for third parties that want
 * to verify lock signatures */
std::string encoded_public_key;
const size_t MAX_MESSAGE_LENGTH = 255;
const size_t TRANSACTION_TABLE_SIZE = 300;
const size_t LOCK_TABLE_SIZE = 10000;

// Struct that gets sealed to storage to persist ECDSA keys
struct DataToSeal {
  sgx_ec256_private_t privateKey;
  sgx_ec256_public_t publicKey;
};

HashTable<std::shared_ptr<Transaction>> transactionTable_(
    TRANSACTION_TABLE_SIZE);
HashTable<std::shared_ptr<Lock>> lockTable_(LOCK_TABLE_SIZE);

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

int register_transaction(unsigned int transactionId, unsigned int lockBudget);

int acquire_lock(void *signature, size_t sig_len, unsigned int transactionId,
                 unsigned int rowId, int isExclusive);

void release_lock(unsigned int transactionId, unsigned int rowId);

/**
 * Releases all locks the given transaction currently has.
 *
 * @param transaction the transaction to be aborted
 */
void abort_transaction(const std::shared_ptr<Transaction> &transaction);

/**
 * @returns the block timeout, which resembles a future block number of the
 *          blockchain in the storage layer. The storage layer will decline
 *          any requests with a signature that has a block timeout number
 *          smaller than the current block number.
 */
auto get_block_timeout() -> unsigned int;