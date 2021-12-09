#pragma once

#include <string>

#include "base64-encoding.h"
#include "enclave_t.h"
#include "sgx_tcrypto.h"
#include "sgx_trts.h"
#include "sgx_tseal.h"

// Context and public private key pair for signing lock requests
extern sgx_ecc_state_handle_t context;
extern sgx_ec256_private_t ec256_private_key;
extern sgx_ec256_public_t ec256_public_key;

// Struct that gets sealed to storage to persist ECDSA keys
struct DataToSeal {
  sgx_ec256_private_t privateKey;
  sgx_ec256_public_t publicKey;
};

const size_t MAX_SIGNATURE_LENGTH = 255;

// Base64 encoded public key
extern std::string encoded_public_key;

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
 * @returns the block timeout, which resembles a future block number of the
 *          blockchain in the storage layer. The storage layer will decline
 *          any requests with a signature that has a block timeout number
 *          smaller than the current block number
 */
auto get_block_timeout() -> int;

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