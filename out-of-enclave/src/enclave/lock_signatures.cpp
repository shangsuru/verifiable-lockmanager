#include "lock_signatures.h"

sgx_ecc_state_handle_t context = NULL;
sgx_ec256_private_t ec256_private_key;
sgx_ec256_public_t ec256_public_key;
std::string encoded_public_key;

auto verify_signature(char *signature, int transactionId, int rowId,
                      int isExclusive) -> int {
  std::string plain = lock_to_string(transactionId, rowId, isExclusive);

  std::string signature_string(signature);
  std::string x = signature_string.substr(0, signature_string.find("-"));
  std::string y = signature_string.substr(signature_string.find("-") + 1,
                                          signature_string.length());
  sgx_ec256_signature_t sig_struct;
  memcpy(sig_struct.x, base64_decode(x).c_str(), 8 * sizeof(uint32_t));
  memcpy(sig_struct.y, base64_decode(y).c_str(), 8 * sizeof(uint32_t));

  int ret =
      verify(plain.c_str(), (void *)&sig_struct, sizeof(sgx_ec256_signature_t));
  if (ret != SGX_SUCCESS) {
    print_error("Failed to verify signature");
  } else {
    print_info("Signature successfully verified");
  }
  return ret;
}

auto lock_to_string(int transactionId, int rowId, bool isExclusive)
    -> std::string {
  unsigned int block_timeout = get_block_timeout();

  std::string mode;
  if (isExclusive) {
    mode = "X";
  } else {
    mode = "S";
  }

  return std::to_string(transactionId) + "_" + std::to_string(rowId) + "_" +
         mode + "_" + std::to_string(block_timeout);
}

auto generate_key_pair() -> int {
  print_info("Creating new key pair");
  sgx_ecc_state_handle_t context;
  sgx_ecc256_open_context(&context);
  sgx_status_t ret = sgx_ecc256_create_key_pair(&ec256_private_key,
                                                &ec256_public_key, context);
  sgx_ecc256_close_context(context);
  encoded_public_key = base64_encode((unsigned char *)&ec256_public_key,
                                     sizeof(ec256_public_key));

  // Append the number of characters for the encoded public key for easy
  // extraction from sealed text file
  encoded_public_key += std::to_string(encoded_public_key.length());
  return ret;
}

auto verify(const char *message, void *signature, size_t sig_len) -> int {
  sgx_ecc_state_handle_t context;
  sgx_ecc256_open_context(&context);
  uint8_t res;
  sgx_ec256_signature_t *sig = (sgx_ec256_signature_t *)signature;
  sgx_status_t ret = sgx_ecdsa_verify((uint8_t *)message,
                                      strnlen(message, MAX_SIGNATURE_LENGTH),
                                      &ec256_public_key, sig, &res, context);
  sgx_ecc256_close_context(context);
  return res;
}

auto get_block_timeout() -> int {
  // TODO: Implement getting the lock timeout
  return 0;
};

auto get_sealed_data_size() -> uint32_t {
  return sgx_calc_sealed_data_size((uint32_t)encoded_public_key.length(),
                                   sizeof(DataToSeal{}));
}

auto seal_keys(uint8_t *sealed_blob, uint32_t sealed_size) -> sgx_status_t {
  print_info("Sealing keys");
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  sgx_sealed_data_t *sealed_data = NULL;
  DataToSeal data;
  data.privateKey = ec256_private_key;
  data.publicKey = ec256_public_key;

  if (sealed_size != 0) {
    sealed_data = (sgx_sealed_data_t *)malloc(sealed_size);
    ret = sgx_seal_data((uint32_t)encoded_public_key.length(),
                        (uint8_t *)encoded_public_key.c_str(), sizeof(data),
                        (uint8_t *)&data, sealed_size, sealed_data);
    if (ret == SGX_SUCCESS)
      memcpy(sealed_blob, sealed_data, sealed_size);
    else
      free(sealed_data);
  }
  return ret;
}

auto unseal_keys(const uint8_t *sealed_blob, size_t sealed_size)
    -> sgx_status_t {
  print_info("Unsealing keys");
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  DataToSeal *unsealed_data = NULL;

  uint32_t dec_size = sgx_get_encrypt_txt_len((sgx_sealed_data_t *)sealed_blob);
  uint32_t mac_text_len =
      sgx_get_add_mac_txt_len((sgx_sealed_data_t *)sealed_blob);

  uint8_t *mac_text = (uint8_t *)malloc(mac_text_len);
  if (dec_size != 0) {
    unsealed_data = (DataToSeal *)malloc(dec_size);
    sgx_sealed_data_t *tmp = (sgx_sealed_data_t *)malloc(sealed_size);
    memcpy(tmp, sealed_blob, sealed_size);
    ret = sgx_unseal_data(tmp, mac_text, &mac_text_len,
                          (uint8_t *)unsealed_data, &dec_size);
    if (ret != SGX_SUCCESS) goto error;
    ec256_private_key = unsealed_data->privateKey;
    ec256_public_key = unsealed_data->publicKey;
  }

error:
  if (unsealed_data != NULL) free(unsealed_data);
  free(mac_text);
  return ret;
}