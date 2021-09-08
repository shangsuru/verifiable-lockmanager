#include <lockmanager.h>

//========= OCALL Functions ===========
void print_info(const char* str) {
  spdlog::info("Enclave: " + std::string{str});
}

void print_error(const char* str) {
  spdlog::error("Enclave: " + std::string{str});
}

void print_warn(const char* str) {
  spdlog::warn("Enclave: " + std::string{str});
}
//=====================================

LockManager::LockManager() {
  if (!initialize_enclave()) {
    spdlog::error("Error at initializing enclave");
  };

  // Generate new keys if keys from sealed storage cannot be found
  int res = -1;
  if (read_and_unseal_keys() == false) {
    generate_key_pair(global_eid, &res);
    if (!seal_and_save_keys()) {
      spdlog::error("Error at sealing keys");
    };
  }
}

LockManager::~LockManager() { sgx_destroy_enclave(global_eid); }

void LockManager::registerTransaction(unsigned int transactionId,
                                      unsigned int lockBudget) {
  int res = -1;
  register_transaction(global_eid, &res, transactionId, lockBudget);
};

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       bool isExclusive) -> std::string {
  int res;
  sgx_ec256_signature_t sig;
  acquire_lock(global_eid, &res, (void*)&sig, sizeof(sgx_ec256_signature_t),
               transactionId, rowId, isExclusive);
  if (res == SGX_ERROR_UNEXPECTED) {
    throw std::domain_error("Acquiring lock failed");
  }

  return base64_encode((unsigned char*)sig.x, sizeof(sig.x)) + "-" +
         base64_encode((unsigned char*)sig.y, sizeof(sig.y));
};

void LockManager::unlock(unsigned int transactionId, unsigned int rowId) {
  int res = -1;
  release_lock(global_eid, transactionId, rowId);
};

auto LockManager::initialize_enclave() -> bool {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL,
                           &global_eid, NULL);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    return false;
  }
  return true;
}

auto LockManager::seal_and_save_keys() -> bool {
  uint32_t sealed_data_size = 0;
  sgx_status_t ret = get_sealed_data_size(global_eid, &sealed_data_size);

  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    return false;
  }

  if (sealed_data_size == UINT32_MAX) {
    return false;
  }

  uint8_t* temp_sealed_buf = (uint8_t*)malloc(sealed_data_size);
  if (temp_sealed_buf == NULL) {
    spdlog::error("Out of memory");
    return false;
  }

  sgx_status_t retval;
  ret = seal_keys(global_eid, &retval, temp_sealed_buf, sealed_data_size);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    free(temp_sealed_buf);
    return false;
  }

  if (retval != SGX_SUCCESS) {
    ret_error_support(retval);
    free(temp_sealed_buf);
    return false;
  }

  // Save the sealed blob
  if (!write_buf_to_file(SEALED_KEY_FILE, temp_sealed_buf, sealed_data_size,
                         0)) {
    spdlog::error("Failed to save the sealed data blob");
    free(temp_sealed_buf);
    return false;
  }

  free(temp_sealed_buf);
  return true;
}

auto LockManager::read_and_unseal_keys() -> bool {
  sgx_status_t ret;
  // Read the sealed blob from the file
  size_t fsize = get_file_size(SEALED_KEY_FILE);
  if (fsize == (size_t)-1) {
    return false;
  }
  uint8_t* temp_buf = (uint8_t*)malloc(fsize);
  if (temp_buf == NULL) {
    spdlog::error("Out of memory");
    return false;
  }
  if (!read_file_to_buf(SEALED_KEY_FILE, temp_buf, fsize)) {
    free(temp_buf);
    return false;
  }

  // Unseal the sealed blob
  sgx_status_t retval;
  ret = unseal_keys(global_eid, &retval, temp_buf, fsize);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    free(temp_buf);
    return false;
  }

  if (retval != SGX_SUCCESS) {
    ret_error_support(retval);
    free(temp_buf);
    return false;
  }

  free(temp_buf);
  return true;
}