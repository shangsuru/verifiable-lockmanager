#include <lockmanager.h>

sgx_enclave_id_t global_eid = 0;
sgx_launch_token_t token = {0};

auto LockManager::load_and_initialize_enclave(sgx_enclave_id_t *eid)
    -> sgx_status_t {
  sgx_status_t ret = SGX_SUCCESS;
  int retval = 0;
  int updated = 0;

  // Check whether the loading and initialization operations are caused
  if (*eid != 0) sgx_destroy_enclave(*eid);

  // Load the enclave
  ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated,
                           eid, NULL);
  if (ret != SGX_SUCCESS) return ret;

  // Save the launch token if updated
  if (updated == 1) {
    std::ofstream ofs(TOKEN_FILENAME, std::ios::binary | std::ios::out);
    if (!ofs.good())
      std::cout << "Warning: Failed to save the launch token to \""
                << TOKEN_FILENAME << "\"" << std::endl;
    else
      ofs << token;
  }
  return ret;
}

auto LockManager::create_worker_thread(void *tmp) -> void * {
  enclave_process_request(global_eid);
  return 0;
}

void LockManager::configuration_init(int numWorkerThreads) {
  const int numLocktableWorkerThreads = numWorkerThreads;
  arg.num_threads =
      numLocktableWorkerThreads + 1;  // one single thread for transaction table
  arg.tx_thread_id = arg.num_threads - 1;
  arg.lock_table_size = 10000;
  arg.transaction_table_size = 200;
}

LockManager::LockManager(int numWorkerThreads) {
  configuration_init(numWorkerThreads);

  // Load and initialize the signed enclave
  sgx_status_t ret = load_and_initialize_enclave(&global_eid);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    // TODO: implement error handling
  }

  enclave_init_values(global_eid, arg);

  // Create worker threads inside the enclave to serve lock requests and
  // registrations of transactions
  threads = (pthread_t *)malloc(sizeof(pthread_t) * (arg.num_threads));
  spdlog::info("Initializing " + std::to_string(arg.num_threads) + " threads");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_create(&threads[i], NULL, &LockManager::create_worker_thread, this);
  }

  // Generate new keys if keys from sealed storage cannot be found
  int res = -1;
  if (read_and_unseal_keys() == false) {
    generate_key_pair(global_eid, &res);
    if (!seal_and_save_keys()) {
      spdlog::error("Error at sealing keys");
    };
  }
}

LockManager::~LockManager() {
  // TODO: Destructor never called (esp. on CTRL+C shutdown)!
  // Send QUIT to worker threads
  create_enclave_job(QUIT, 0, 0, 0, false);

  spdlog::info("Waiting for thread to stop");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  spdlog::info("Freeing threads");
  free(threads);

  spdlog::info("Destroying enclave");
  sgx_destroy_enclave(global_eid);
}

auto LockManager::registerTransaction(unsigned int transactionId,
                                      unsigned int lockBudget) -> bool {
  return create_enclave_job(REGISTER, transactionId, 0, lockBudget).second;
};

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       bool isExclusive, bool waitForResult)
    -> std::pair<std::string, bool> {
  if (isExclusive) {
    return create_enclave_job(EXCLUSIVE, transactionId, rowId, 0,
                              waitForResult);
  }
  return create_enclave_job(SHARED, transactionId, rowId, 0, waitForResult);
};

void LockManager::unlock(unsigned int transactionId, unsigned int rowId,
                         bool waitForResult) {
  create_enclave_job(UNLOCK, transactionId, rowId, 0, waitForResult);
};

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

  uint8_t *temp_sealed_buf = (uint8_t *)malloc(sealed_data_size);
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
  uint8_t *temp_buf = (uint8_t *)malloc(fsize);
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

auto LockManager::create_enclave_job(Command command,
                                     unsigned int transaction_id,
                                     unsigned int row_id,
                                     unsigned int lock_budget,
                                     bool waitForResult)
    -> std::pair<std::string, bool> {
  // Set job parameters
  Job job;
  job.command = command;

  job.transaction_id = transaction_id;
  job.row_id = row_id;
  job.lock_budget = lock_budget;

  if (waitForResult) {  // Need to track, when job is finished or error has
                        // occurred
    // Allocate dynamic memory in the untrusted part of the application, so the
    // enclave can modify it via its pointer
    job.finished = new bool;
    job.error = new bool;
    *job.finished = false;
    *job.error = false;
  }

  if (command == SHARED || command == EXCLUSIVE) {
    // These requests return a signature
    job.return_value = new char[SIGNATURE_SIZE];
  }
  job.wait_for_result = waitForResult;
  enclave_send_job(global_eid, &job);

  if (waitForResult) {
    // Need to wait until job is finished because we need to be registered for
    // subsequent requests or because we need to wait for the return value
    while (!*job.finished) {
      continue;
    }
    delete job.finished;

    // Check if an error occured
    if (*job.error) {
      return std::make_pair(NO_SIGNATURE, false);
    }
    delete job.error;

    // Get the signature return value
    if (command == SHARED || command == EXCLUSIVE) {
      std::string signature;
      for (int i = 0; i < SIGNATURE_SIZE; i++) {
        signature += job.return_value[i];
      }
      delete[] job.return_value;

      return std::make_pair(signature, true);
    }
  }

  return std::make_pair(NO_SIGNATURE, true);
}

auto LockManager::verify_signature_string(std::string signature,
                                          int transactionId, int rowId,
                                          int isExclusive) -> bool {
  int res = SGX_SUCCESS;
  verify_signature(global_eid, &res, (char *)signature.c_str(), transactionId,
                   rowId, isExclusive);
  if (res != SGX_SUCCESS) {
    print_error("Failed to verify signature");
    return false;
  } else {
    print_debug("Signature successfully verified");
    return true;
  }
}