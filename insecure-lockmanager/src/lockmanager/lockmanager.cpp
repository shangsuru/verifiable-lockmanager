#include <lockmanager.h>

int num = 0;  // global variable used to give every thread a unique ID
size_t transactionTableSize_;
size_t lockTableSize_;

auto LockManager::load_and_initialize_threads(void *object) -> void * {
  reinterpret_cast<LockManager *>(object)->worker_thread();
  return 0;
}

void LockManager::configuration_init() {
  arg.num_threads = 2;
  arg.tx_thread_id = arg.num_threads - 1;
}

LockManager::LockManager() {
  configuration_init();

  // Get configuration parameters
  transactionTableSize_ = arg.transaction_table_size;
  lockTableSize_ = arg.lock_table_size;

  // Initialize mutex variables
  pthread_mutex_init(&global_mutex, NULL);
  queue_mutex =
      (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * arg.num_threads);
  job_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) * arg.num_threads);

  // Initialize job queues
  for (int i = 0; i < arg.num_threads; i++) {
    queue.push_back(std::queue<Job>());
  }

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
      break;

    case SHARED:
    case EXCLUSIVE:
    case UNLOCK: {
      // Copy job parameters
      new_job.transaction_id = ((Job *)data)->transaction_id;
      new_job.row_id = ((Job *)data)->row_id;
      new_job.finished = ((Job *)data)->finished;
      new_job.error = ((Job *)data)->error;

      // Send the requests to specific worker thread
      int thread_id = (int)((new_job.row_id % lockTableSize_) /
                            (lockTableSize_ / (arg.num_threads - 1)));
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

      // Send the requests to thread responsible for registering transactions
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

void LockManager::worker_thread() {
  pthread_mutex_lock(&global_mutex);

  int thread_id = num;
  num += 1;

  pthread_mutex_init(&queue_mutex[thread_id], NULL);
  pthread_cond_init(&job_cond[thread_id], NULL);

  pthread_mutex_unlock(&global_mutex);

  pthread_mutex_lock(&queue_mutex[thread_id]);

  while (1) {
    spdlog::info("Worker waiting for jobs");
    if (queue[thread_id].size() == 0) {
      pthread_cond_wait(&job_cond[thread_id], &queue_mutex[thread_id]);
      continue;
    }

    spdlog::info("Worker got a job");
    Job cur_job = queue[thread_id].front();
    Command command = cur_job.command;

    pthread_mutex_unlock(&queue_mutex[thread_id]);

    switch (command) {
      case QUIT:
        pthread_mutex_lock(&queue_mutex[thread_id]);
        queue[thread_id].pop();
        pthread_mutex_unlock(&queue_mutex[thread_id]);
        pthread_mutex_destroy(&queue_mutex[thread_id]);
        pthread_cond_destroy(&job_cond[thread_id]);
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

        if (!ok) {
          *cur_job.error = true;
        }

        *cur_job.finished = true;
        break;
      }
      case UNLOCK: {
        spdlog::info(
            ("(UNLOCK) TXID: " + std::to_string(cur_job.transaction_id) +
             ", RID: " + std::to_string(cur_job.row_id))
                .c_str());
        release_lock(cur_job.transaction_id, cur_job.row_id);
        break;
      }
      case REGISTER: {
        auto transactionId = cur_job.transaction_id;

        spdlog::info(
            ("Registering transaction " + std::to_string(transactionId))
                .c_str());

        if (transactionTable_.find(transactionId) != transactionTable_.end()) {
          spdlog::error("Transaction is already registered");
          *cur_job.error = true;
        } else {
          transactionTable_[transactionId] = new Transaction(transactionId);
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
  auto transaction = transactionTable_[transactionId];
  if (transaction == nullptr) {
    spdlog::error("Transaction was not registered");
    return false;
  }

  // Get the lock object for the given row ID
  auto lock = lockTable_[rowId];
  if (lock == nullptr) {
    lock = new Lock();
    lockTable_[rowId] = lock;
  }

  // Check if 2PL is violated
  if (transaction->getPhase() == Transaction::Phase::kShrinking) {
    spdlog::error("Cannot acquire more locks according to 2PL");
    goto abort;
  }

  // Check for upgrade request
  if (transaction->hasLock(rowId) && isExclusive &&
      lock->getMode() == Lock::LockMode::kShared) {
    ret = lock->upgrade(transactionId);

    if (!ret) {
      goto abort;
    }
    return ret;
  }

  // Acquire lock in requested mode (shared, exclusive)
  if (!transaction->hasLock(rowId)) {
    if (isExclusive) {
      ret = transaction->addLock(rowId, Lock::LockMode::kExclusive, lock);
    } else {
      ret = transaction->addLock(rowId, Lock::LockMode::kShared, lock);
    }

    if (!ret) {
      goto abort;
    }

    return ret;
  }

  spdlog::error("Request for already acquired lock");

abort:
  abort_transaction(transaction);
  return false;
}

void LockManager::release_lock(unsigned int transactionId, unsigned int rowId) {
  // Get the transaction object
  auto transaction = transactionTable_[transactionId];
  if (transaction == nullptr) {
    spdlog::error("Transaction was not registered");
    return;
  }

  // Get the lock object
  auto lock = lockTable_[rowId];
  if (lock == nullptr) {
    spdlog::error("Lock does not exist");
    return;
  }

  transaction->releaseLock(rowId, lockTable_);
}

void LockManager::abort_transaction(Transaction *transaction) {
  transactionTable_.erase(transaction->getTransactionId());
  transaction->releaseAllLocks(lockTable_);
  delete transaction;
}