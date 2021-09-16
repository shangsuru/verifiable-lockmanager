#include "enclave.h"

Arg arg_enclave;  // configuration parameters for the enclave
int num = 0;      // global variable used to give every thread a unique ID
pthread_mutex_t global_mutex;  // synchronizes access to num
pthread_mutex_t *queue_mutex;  // synchronizes access to the job queue
pthread_cond_t
    *job_cond;  // wakes up worker threads when a new job is available
std::vector<std::queue<Job>> queue;  // a job queue for each worker thread

// Holds the transaction objects of the currently active transactions
std::unordered_map<unsigned int, Transaction *> transactionTable_;
size_t transactionTableSize_;

// Keeps track of a lock object for each row ID
std::unordered_map<unsigned int, Lock *> lockTable_;
size_t lockTableSize_;

void init_values(Arg arg) {
  // Get configuration parameters
  arg_enclave = arg;
  transactionTableSize_ = arg.transaction_table_size;
  lockTableSize_ = arg.lock_table_size;

  // Initialize mutex variables
  pthread_mutex_init(&global_mutex, NULL);
  queue_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) *
                                          arg_enclave.num_threads);
  job_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) *
                                      arg_enclave.num_threads);

  // Initialize job queues
  for (int i = 0; i < arg_enclave.num_threads; i++) {
    queue.push_back(std::queue<Job>());
  }
}

void send_job(void *data) {
  Command command = ((Job *)data)->command;
  Job new_job;
  new_job.command = command;

  switch (command) {
    case QUIT:
      // Send exit message to all of the worker threads
      for (int i = 0; i < arg_enclave.num_threads; i++) {
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
                            (lockTableSize_ / (arg_enclave.num_threads - 1)));
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
      pthread_mutex_lock(&queue_mutex[arg_enclave.tx_thread_id]);
      queue[arg_enclave.tx_thread_id].push(new_job);
      pthread_cond_signal(&job_cond[arg_enclave.tx_thread_id]);
      pthread_mutex_unlock(&queue_mutex[arg_enclave.tx_thread_id]);
      break;
    }
    default:
      spdlog::error("Received unknown command");
      break;
  }
}

void worker_thread() {
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

auto acquire_lock(unsigned int transactionId, unsigned int rowId,
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

void release_lock(unsigned int transactionId, unsigned int rowId) {
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

void abort_transaction(Transaction *transaction) {
  transactionTable_.erase(transaction->getTransactionId());
  transaction->releaseAllLocks(lockTable_);
  delete transaction;
}