#pragma once

#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include "sgx_tcrypto.h"
#include "sgx_tseal.h"
#include "Base64Encoding.h"
#include "Enclave_t.h"

sgx_ecc_state_handle_t context = NULL;
sgx_ec256_private_t ec256_private_key;
sgx_ec256_public_t ec256_public_key;
const size_t MAX_MESSAGE_LENGTH = 255;

// Struct that gets sealed to storage to persist ECDSA keys
struct DataToSeal
{
  sgx_ec256_private_t privateKey;
  sgx_ec256_public_t publicKey;
};

/**
 * @returns the size of the encrypted DataToSeal struct
 */
uint32_t get_sealed_data_size();

/**
 * Seals the public and private key and stores it inside the sealed blob.
 * @param sealed_blob buffer to store sealed keys
 * @param sealed_size size of the sealed blob buffer
 * @returns SGX_SUCCESS or error code
 */
sgx_status_t seal_keys(uint8_t *sealed_blob, uint32_t sealed_size);

/**
 * Unseals the keys stored in sealed_blob and sets global private and 
 * public key attribute.
 * @param sealed_blob buffer that contains the sealed public and private keys
 * @param sealed_size size of the sealed blob buffer
 * @returns SGX_SUCCESS or error code
 */
sgx_status_t unseal_keys(const uint8_t *sealed_blob, size_t data_size);

/**
 * Generates keys for ECDSA signature and sets corresponding private and 
 * public key attribute.
 * @returns SGX_SUCCESS or error code
 */
int generate_key_pair();

/**
 * Signs the message and stores the signature in given buffer.
 * @param message message to sign
 * @param signature buffer to store the signature
 * @param sig_len size of signature
 * @returns SGX_SUCCESS or error code
 */
int sign(const char *message, void *signature, size_t sig_len);

/**
 * Verifies given message signature pair.
 * @param message message to verify
 * @param signature signature of message
 * @param sig_len size of signature
 * @returns SGX_SUCCESS or error code
 */
int verify(const char *message, void *signature, size_t sig_len);

/**
 * Closes the ECDSA context.
 * @returns SGX_SUCCESS or error code
 */
int ecdsa_close();