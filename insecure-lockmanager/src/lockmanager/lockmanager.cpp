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

auto LockManager::load_and_initialize_threads(void *tmp) -> void * {
  enclave_worker_thread(global_eid);
  return 0;
}

void LockManager::configuration_init() {
  arg.num_threads = 2;
  arg.tx_thread_id = arg.num_threads - 1;
}

LockManager::LockManager() {
  configuration_init();

  // Load and initialize the signed enclave
  sgx_status_t ret = load_and_initialize_enclave(&global_eid);
  if (ret != SGX_SUCCESS) {
    ret_error_support(ret);
    // TODO: implement error handling
  }

  enclave_init_values(global_eid, arg);

  // Create threads for lock requests and an additional one for registering
  // transactions
  threads = (pthread_t *)malloc(sizeof(pthread_t) * (arg.num_threads));
  spdlog::info("Initializing " + std::to_string(arg.num_threads) + " threads");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_create(&threads[i], NULL, &LockManager::load_and_initialize_threads,
                   this);
  }
}

LockManager::~LockManager() {
  // TODO: Destructor never called (esp. on CTRL+C shutdown)!
  // Send QUIT to worker threads
  create_job(QUIT);

  spdlog::info("Waiting for thread to stop");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  spdlog::info("Freing threads");
  free(threads);

  spdlog::info("Destroying enclave");
  sgx_destroy_enclave(global_eid);
}

void LockManager::registerTransaction(unsigned int transactionId) {
  create_job(REGISTER, transactionId, 0);
};

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       bool isExclusive) -> bool {
  if (isExclusive) {
    return create_job(EXCLUSIVE, transactionId, rowId);
  }
  return create_job(SHARED, transactionId, rowId);
};

void LockManager::unlock(unsigned int transactionId, unsigned int rowId) {
  create_job(UNLOCK, transactionId, rowId);
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

auto LockManager::create_job(Command command, unsigned int transaction_id,
                             unsigned int row_id) -> bool {
  // Set job parameters
  Job job;
  job.command = command;

  job.transaction_id = transaction_id;
  job.row_id = row_id;

  if (command == SHARED || command == EXCLUSIVE || command == REGISTER) {
    // Need to track, when job is finished
    job.finished = new bool;
    *job.finished = false;
    // Need to track, if an error occured
    job.error = new bool;
    *job.error = false;
  }

  enclave_send_job(global_eid, &job);

  if (command == SHARED || command == EXCLUSIVE || command == REGISTER) {
    // Need to wait until job is finished because we need to be registered for
    // subsequent requests or because we need to wait for the return value
    while (!*job.finished) {
      continue;
    }
    delete job.finished;

    // Check if an error occured
    if (*job.error) {
      return false;
    }
    delete job.error;
  }

  return true;
}