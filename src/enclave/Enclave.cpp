#include "Enclave.h"

uint32_t get_sealed_data_size()
{
  return sgx_calc_sealed_data_size(NULL, sizeof(DataToSeal{}));
}

sgx_status_t seal_keys(uint8_t *sealed_blob, uint32_t sealed_size)
{
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  sgx_sealed_data_t *sealed_data = NULL;
  DataToSeal data;
  data.privateKey = ec256_private_key;
  data.publicKey = ec256_public_key;

  if (sealed_size != 0)
  {
    sealed_data = (sgx_sealed_data_t *)malloc(sealed_size);
    ret = sgx_seal_data(NULL, NULL, sizeof(data), (uint8_t *)&data, sealed_size, sealed_data);
    if (ret == SGX_SUCCESS)
      memcpy(sealed_blob, sealed_data, sealed_size);
    else
      free(sealed_data);
  }
  return ret;
}

sgx_status_t unseal_keys(const uint8_t *sealed_blob, size_t sealed_size)
{
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  DataToSeal *unsealed_data = NULL;

  uint32_t dec_size = sgx_get_encrypt_txt_len((sgx_sealed_data_t *)sealed_blob);
  if (dec_size != 0)
  {
    unsealed_data = (DataToSeal *)malloc(dec_size);
    sgx_sealed_data_t *tmp = (sgx_sealed_data_t *)malloc(sealed_size);
    memcpy(tmp, sealed_blob, sealed_size);
    ret = sgx_unseal_data(tmp, NULL, NULL, (uint8_t *)unsealed_data, &dec_size);
    if (ret != SGX_SUCCESS)
      goto error;
    ec256_private_key = unsealed_data->privateKey;
    ec256_public_key = unsealed_data->publicKey;
  }

error:
  if (unsealed_data != NULL)
    free(unsealed_data);
  return ret;
}

int generate_key_pair()
{
  if (context == NULL)
    sgx_ecc256_open_context(&context);
  return sgx_ecc256_create_key_pair(&ec256_private_key, &ec256_public_key, context);
}

// Signs a given message and returns the signature object
int sign(const char *message, void *signature, size_t sig_len)
{
  if (context == NULL)
    sgx_ecc256_open_context(&context);
  return sgx_ecdsa_sign((uint8_t *)message, strnlen(message, MAX_MESSAGE_LENGTH), &ec256_private_key, (sgx_ec256_signature_t *)signature, context);
}

// Verifies a given message with its signature object and returns on success SGX_EC_VALID or on failure SGX_EC_INVALID_SIGNATURE
int verify(const char *message, void *signature, size_t sig_len)
{
  if (context == NULL)
    sgx_ecc256_open_context(&context);
  uint8_t res;
  sgx_ec256_signature_t *sig = (sgx_ec256_signature_t *)signature;
  sgx_status_t ret = sgx_ecdsa_verify((uint8_t *)message, strnlen(message, MAX_MESSAGE_LENGTH), &ec256_public_key, sig, &res, context);
  return res;
}

// Closes the ECDSA context
int ecdsa_close()
{
  if (context == NULL)
    sgx_ecc256_open_context(&context);
  return sgx_ecc256_close_context(context);
}