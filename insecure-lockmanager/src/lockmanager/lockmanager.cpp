#include <lockmanager.h>

size_t transactionTableSize_;
size_t lockTableSize_;
auto LockManager::create_worker_thread(void *object) -> void * {
  reinterpret_cast<LockManager *>(object)->process_request();
  return 0;
}

void LockManager::configuration_init(int numWorkerThreads) {
  const int numLocktableWorkerThreads = numWorkerThreads;
  arg.num_threads =
      numLocktableWorkerThreads + 1;  // + 1 thread for transaction table;
  arg.tx_thread_id = arg.num_threads - 1;
  arg.transaction_table_size = 200;
  arg.lock_table_size = 10000;
}

LockManager::LockManager(int numWorkerThreads) {
  configuration_init(numWorkerThreads);

  // Get configuration parameters
  transactionTableSize_ = arg.transaction_table_size;
  lockTableSize_ = arg.lock_table_size;

  transactionTable_ = newHashTable(transactionTableSize_);
  lockTable_ = newHashTable(lockTableSize_);

  // Initialize mutex variables
  pthread_mutex_init(&global_num_mutex, NULL);
  queue_mutex =
      (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * arg.num_threads);
  job_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) * arg.num_threads);

  // Initialize job queues
  for (int i = 0; i < arg.num_threads; i++) {
    queue.push_back(std::queue<Job>());
  }

  // Create worker threads to serve lock requests and registrations of
  // transactions
  threads = (pthread_t *)malloc(sizeof(pthread_t) * (arg.num_threads));
  spdlog::info("Initializing " + std::to_string(arg.num_threads) + " threads");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_mutex_init(&queue_mutex[i], NULL);
    pthread_cond_init(&job_cond[i], NULL);
    pthread_create(&threads[i], NULL, &LockManager::create_worker_thread, this);
  }
}

// TODO: Destructor never called (esp. on CTRL+C shutdown)!
LockManager::~LockManager() {
  // Send QUIT to worker threads
  create_job(QUIT);

  spdlog::info("Waiting for thread to stop");
  for (int i = 0; i < arg.num_threads; i++) {
    pthread_join(threads[i], NULL);
    pthread_mutex_destroy(&queue_mutex[i]);
    pthread_cond_destroy(&job_cond[i]);
  }

  spdlog::info("Freeing threads");
  free(threads);
}

auto LockManager::registerTransaction(unsigned int transactionId) -> bool {
  return create_job(REGISTER, transactionId);
};

auto LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       bool isExclusive, bool waitForResult) -> bool {
  if (isExclusive) {
    return create_job(EXCLUSIVE, transactionId, rowId, waitForResult);
  }
  return create_job(SHARED, transactionId, rowId, waitForResult);
};

void LockManager::unlock(unsigned int transactionId, unsigned int rowId,
                         bool waitForResult) {
  create_job(UNLOCK, transactionId, rowId, waitForResult);
};

auto LockManager::create_job(Command command, unsigned int transaction_id,
                             unsigned int row_id, bool waitForResult) -> bool {
  // Set job parameters
  Job job;
  job.command = command;

  job.transaction_id = transaction_id;
  job.row_id = row_id;

  // Need to track, when job is finished or error has occurred
  if (waitForResult && (command == SHARED || command == EXCLUSIVE ||
                        command == REGISTER || command == UNLOCK)) {
    // Allocate dynamic memory in the untrusted part of the application, so the
    // enclave can modify it via its pointer
    job.finished = new bool;
    job.error = new bool;
    *job.finished = false;
    *job.error = false;
  }

  job.wait_for_result = waitForResult;
  send_job(&job);
  if (waitForResult && (command == SHARED || command == EXCLUSIVE ||
                        command == REGISTER || command == UNLOCK)) {
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

void LockManager::send_job(void *data) {
  Command command = ((Job *)data)->command;
  Job new_job;
  new_job.command = command;

  switch (command) {
    case QUIT:
      // Send exit message to all of the worker threads
      for (int i = 0; i < arg.num_threads; i++) {
        spdlog::info("Sending QUIT to all threads");

        pthread_mutex_lock(&queue_mutex[i]);
        queue[i].push(new_job);
        pthread_cond_signal(&job_cond[i]);
        pthread_mutex_unlock(&queue_mutex[i]);
      }
      return;

    case SHARED:
    case EXCLUSIVE:
    case UNLOCK: {
      // Copy job parameters

      new_job.transaction_id = ((Job *)data)->transaction_id;
      new_job.row_id = ((Job *)data)->row_id;
      new_job.wait_for_result = ((Job *)data)->wait_for_result;

      if (new_job.wait_for_result) {
        new_job.finished = ((Job *)data)->finished;
        new_job.error = ((Job *)data)->error;
      }
      // If transaction is not registered, abort the request
      if (get(transactionTable_, new_job.transaction_id) == nullptr) {
        spdlog::error("Need to register transaction before lock requests");
        if (new_job.wait_for_result) {
          *new_job.error = true;
          *new_job.finished = true;
        }
        return;
      }

      // Send the requests to specific worker thread
      int thread_id = (int)((new_job.row_id % lockTableSize_) /
                            ((float)lockTableSize_ / (arg.num_threads - 1)));
      pthread_mutex_lock(&queue_mutex[thread_id]);
      queue[thread_id].push(new_job);
      pthread_cond_signal(&job_cond[thread_id]);
      pthread_mutex_unlock(&queue_mutex[thread_id]);
      break;
    }
    case REGISTER: {
      // Copy job parameters
      new_job.transaction_id = ((Job *)data)->transaction_id;
      new_job.finished = ((Job *)data)->finished;
      new_job.error = ((Job *)data)->error;

      // Send the requests to thread responsible for registering
      // transactions
      pthread_mutex_lock(&queue_mutex[arg.tx_thread_id]);
      queue[arg.tx_thread_id].push(new_job);
      pthread_cond_signal(&job_cond[arg.tx_thread_id]);
      pthread_mutex_unlock(&queue_mutex[arg.tx_thread_id]);
      break;
    }
    default:
      spdlog::error("Received unknown command");
      break;
  }
}

void LockManager::process_request() {
  pthread_mutex_lock(&global_num_mutex);

  int thread_id = num;
  num += 1;

  pthread_mutex_unlock(&global_num_mutex);

  pthread_mutex_lock(&queue_mutex[thread_id]);
  while (1) {
    spdlog::info("Worker " + std::to_string(thread_id) + " waiting for jobs");
    if (queue[thread_id].size() == 0) {
      pthread_cond_wait(&job_cond[thread_id], &queue_mutex[thread_id]);
      continue;
    }

    spdlog::info("Worker " + std::to_string(thread_id) + " got a job");
    Job cur_job = queue[thread_id].front();
    Command command = cur_job.command;

    pthread_mutex_unlock(&queue_mutex[thread_id]);

    switch (command) {
      case QUIT:
        pthread_mutex_lock(&queue_mutex[thread_id]);
        queue[thread_id].pop();
        pthread_mutex_unlock(&queue_mutex[thread_id]);
        spdlog::info("Enclave worker quitting");
        return;
      case SHARED:
      case EXCLUSIVE: {
        if (command == EXCLUSIVE) {
          spdlog::info(
              ("(EXCLUSIVE) TXID: " + std::to_string(cur_job.transaction_id) +
               ", RID: " + std::to_string(cur_job.row_id))
                  .c_str());
        } else {
          spdlog::info(
              ("(SHARED) TXID: " + std::to_string(cur_job.transaction_id) +
               ", RID: " + std::to_string(cur_job.row_id))
                  .c_str());
        }

        bool ok = acquire_lock(cur_job.transaction_id, cur_job.row_id,
                               command == EXCLUSIVE);

        if (cur_job.wait_for_result) {
          if (!ok) {
            *cur_job.error = true;
          }

          *cur_job.finished = true;
        }
        break;
      }
      case UNLOCK: {
        spdlog::info(
            ("(UNLOCK) TXID: " + std::to_string(cur_job.transaction_id) +
             ", RID: " + std::to_string(cur_job.row_id))
                .c_str());
        release_lock(cur_job.transaction_id, cur_job.row_id);
        if (cur_job.wait_for_result) {
          *cur_job.finished = true;
        }
        break;
      }
      case REGISTER: {
        auto transactionId = cur_job.transaction_id;

        spdlog::info(
            ("Registering transaction " + std::to_string(transactionId))
                .c_str());

        if (contains(transactionTable_, transactionId)) {
          spdlog::error("Transaction is already registered");
          *cur_job.error = true;
        } else {
          set(transactionTable_, transactionId, newTransaction(transactionId));
        }
        *cur_job.finished = true;
        break;
      }
      default:
        spdlog::error("Worker received unknown command");
    }

    pthread_mutex_lock(&queue_mutex[thread_id]);
    queue[thread_id].pop();
  }

  return;
}

auto LockManager::acquire_lock(unsigned int transactionId, unsigned int rowId,
                               bool isExclusive) -> bool {
  int ret;

  // Get the transaction object for the given transaction ID
  auto transaction = (Transaction *)get(transactionTable_, transactionId);
  if (transaction == nullptr) {
    spdlog::error("Transaction was not registered");
    return false;
  }

  // Get the lock object for the given row ID
  auto lock = (Lock *)get(lockTable_, rowId);
  if (lock == nullptr) {
    lock = newLock();
    set(lockTable_, rowId, (void *)lock);
  }

  // Check if 2PL is violated
  if (!transaction->growing_phase) {
    spdlog::error("Cannot acquire more locks according to 2PL");
    abort_transaction(transaction);
    return false;
  }

  // Comment out for evaluation ->
  // Check for upgrade request
  if (hasLock(transaction, rowId) && isExclusive && !lock->exclusive) {
    ret = upgrade(lock, transactionId);

    if (!ret) {
      abort_transaction(transaction);
      return false;
    }
    return ret;
  }

  // Acquire lock in requested mode (shared, exclusive)
  if (!hasLock(transaction, rowId)) {
    // <- Comment out for evaluation
    ret = addLock(transaction, rowId, isExclusive, lock);
    if (!ret) {
      abort_transaction(transaction);
      return false;
    }
    return ret;
    // Comment out for evaluation ->
  }
  // <- Comment out for evaluation

  spdlog::error("Request for already acquired lock");
  abort_transaction(transaction);
  return false;
}

void LockManager::release_lock(unsigned int transactionId, unsigned int rowId) {
  // Get the transaction object
  auto transaction = (Transaction *)get(transactionTable_, transactionId);
  if (transaction == nullptr) {
    spdlog::error("Transaction was not registered");
    return;
  }

  // Get the lock object
  auto lock = (Lock *)get(lockTable_, rowId);
  if (lock == nullptr) {
    spdlog::error("Lock does not exist");
    return;
  }

  releaseLock(transaction, rowId, lockTable_);

  // If the transaction released its last lock, delete it
  if (transaction->locked_rows.size() == 0) {
    remove(transactionTable_, transactionId);
    delete transaction;
  }
}

void LockManager::abort_transaction(Transaction *transaction) {
  remove(transactionTable_, transaction->transaction_id);
  releaseAllLocks(transaction, lockTable_);
  delete transaction;
}