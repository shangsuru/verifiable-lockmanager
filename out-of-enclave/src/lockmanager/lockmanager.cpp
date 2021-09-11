#include <lockmanager.h>

/**
 * load and initialize the enclave
 **/
auto LockManager::load_and_initialize_enclave(sgx_enclave_id_t *eid)
    -> sgx_status_t {
  sgx_status_t ret = SGX_SUCCESS;
  int retval = 0;
  int updated = 0;

  // Step 1: check whether the loading and initialization operations are caused
  if (*eid != 0) sgx_destroy_enclave(*eid);

  // Step 2: load the enclave
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

/**
 * For mac bucketing optimization
 * create mac buffer
 **/
auto LockManager::macbuffer_create(int size) -> MACbuffer * {
  MACbuffer *Mbuf = NULL;

  Mbuf = (MACbuffer *)malloc(sizeof(MACbuffer));
  Mbuf->entry = (MACentry *)malloc(sizeof(MACentry) * size);
  for (int i = 0; i < size; i++) {
    Mbuf->entry[i].size = 0;
  }
  return Mbuf;
}

/**
 * create new hashtable
 **/
auto LockManager::ht_create(int size) -> hashtable * {
  hashtable *ht = NULL;

  if (size < 1) return NULL;

  /* Allocate the table itself. */
  if ((ht = (hashtable *)malloc(sizeof(hashtable))) == NULL) return NULL;

  /* Allocate pointers to the head nodes. */
  if ((ht->table = (entry **)malloc(sizeof(entry *) * size)) == NULL)
    return NULL;

  for (int i = 0; i < size; i++) ht->table[i] = NULL;

  ht->size = size;

  return ht;
}

/**
 * creating server threads working inside the enclave
 **/
auto LockManager::load_and_initialize_threads(void *object) -> void * {
  reinterpret_cast<LockManager *>(object)->start_worker_threads();
  return 0;
}

void LockManager::start_worker_threads() {
  enclave_worker_thread(global_eid, (hashtable *)ht, (MACbuffer *)MACbuf);
}

//========= OCALL Functions ===========
void print_info(const char *str) {
  spdlog::info("Enclave: " + std::string{str});
}

void print_error(const char *str) {
  spdlog::error("Enclave: " + std::string{str});
}

void print_warn(const char *str) {
  spdlog::warn("Enclave: " + std::string{str});
}
//=====================================

/**
 * init default configuration values
 **/
void LockManager::configuration_init() {
  arg.max_buf_size = 64;
  arg.num_threads = 1;

  arg.bucket_size = 8 * 1024 * 1024;
  arg.tree_root_size = 4 * 1024 * 1024;

  /** Optimization **/
  arg.key_opt = false;
  arg.mac_opt = false;
}

LockManager::LockManager() {
  configuration_init();

  // Load and initialize the signed enclave
  sgx_status_t ret = load_and_initialize_enclave(&global_eid);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    // TODO: implement error handling
  }

  ht = ht_create(arg.bucket_size);
  MACbuf = macbuffer_create(arg.bucket_size);

  // Initialize hash tree
  enclave_init_values(global_eid, ht, MACbuf, arg);

  // Create worker threads
  threads = (pthread_t *)malloc(sizeof(pthread_t) * (arg.num_threads));
  spdlog::info("Initializing " + std::to_string(arg.num_threads) + " threads");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_create(&threads[i], NULL, &LockManager::load_and_initialize_threads,
                   this);
  }

  // Generate new keys if keys from sealed storage cannot be found
  int res = -1;
  if (read_and_unseal_keys() == false) {
    generate_key_pair(global_eid, &res);
    if (!seal_and_save_keys()) {
      spdlog::error("Error at sealing keys");
    };
  }
  //===============TEST====================
  // Send SHARED to worker threads
  job job1;
  job1.command = SHARED;
  job1.signature = (char *)malloc(sizeof(char));
  memset(job1.signature, 0, 1);

  enclave_message_pass(global_eid, &job1);

  job job2;
  job2.command = EXCLUSIVE;
  job2.signature = (char *)malloc(sizeof(char));
  memset(job2.signature, 0, 1);

  enclave_message_pass(global_eid, &job2);

  job job3;
  job3.command = UNLOCK;
  job3.signature = (char *)malloc(sizeof(char));
  memset(job3.signature, 0, 1);

  enclave_message_pass(global_eid, &job3);

  // Send QUIT to worker threads
  job job4;
  job4.command = QUIT;
  job4.signature = (char *)malloc(sizeof(char));
  memset(job4.signature, 0, 1);

  enclave_message_pass(global_eid, &job4);
  while (strncmp(job4.signature, "x", 1) != 0) {
    spdlog::info("====" + std::to_string(job4.signature[0]) +
                 "====");  // TODO: It doesn't work without this line, why? Use
                           // condition variables!
    continue;
  }

  std::string return_value;
  return_value += job4.signature[0];
  spdlog::info("====" + return_value + "====");

  spdlog::info("Waiting for thread to stop");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  spdlog::info("Freing threads");
  free(threads);
}

LockManager::~LockManager() {
  // TODO: Destructor never called (esp. on CTRL+C shutdown)!

  spdlog::info("Destroying enclave");
  sgx_destroy_enclave(global_eid);
}

void LockManager::registerTransaction(unsigned int transactionId,
                                      unsigned int lockBudget) {
  int res = -1;
  register_transaction(global_eid, &res, transactionId, lockBudget);
};

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       bool isExclusive) -> std::string {
  int res;
  sgx_ec256_signature_t sig;
  acquire_lock(global_eid, &res, (void *)&sig, sizeof(sgx_ec256_signature_t),
               transactionId, rowId, isExclusive);
  if (res == SGX_ERROR_UNEXPECTED) {
    throw std::domain_error("Acquiring lock failed");
  }

  return base64_encode((unsigned char *)sig.x, sizeof(sig.x)) + "-" +
         base64_encode((unsigned char *)sig.y, sizeof(sig.y));
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