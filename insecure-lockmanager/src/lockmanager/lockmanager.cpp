#include <lockmanager.h>

auto LockManager::load_and_initialize_threads(void *tmp) -> void * {
  worker_thread();
  return 0;
}

void LockManager::configuration_init() {
  arg.num_threads = 2;
  arg.tx_thread_id = arg.num_threads - 1;
}

LockManager::LockManager() {
  configuration_init();

  init_values(arg);

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

  send_job(&job);

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