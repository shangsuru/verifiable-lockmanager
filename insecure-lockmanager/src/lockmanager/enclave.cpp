#include "enclave.h"

Arg arg_enclave;  // configuration parameters for the enclave
int num = 0;      // global variable used to give every thread a unique ID
sgx_thread_mutex_t global_mutex;  // synchronizes access to num
sgx_thread_mutex_t *queue_mutex;  // synchronizes access to the job queue
sgx_thread_cond_t
    *job_cond;  // wakes up worker threads when a new job is available
std::vector<std::queue<Job>> queue;  // a job queue for each worker thread

void enclave_init_values(Arg arg) {
  // Get configuration parameters
  arg_enclave = arg;
  transactionTableSize_ = arg.transaction_table_size;
  lockTableSize_ = arg.lock_table_size;

  // Initialize mutex variables
  sgx_thread_mutex_init(&global_mutex, NULL);
  queue_mutex = (sgx_thread_mutex_t *)malloc(sizeof(sgx_thread_mutex_t) *
                                             arg_enclave.num_threads);
  job_cond = (sgx_thread_cond_t *)malloc(sizeof(sgx_thread_cond_t) *
                                         arg_enclave.num_threads);

  // Initialize job queues
  for (int i = 0; i < arg_enclave.num_threads; i++) {
    queue.push_back(std::queue<Job>());
  }
}

void enclave_send_job(void *data) {
  Command command = ((Job *)data)->command;
  Job new_job;
  new_job.command = command;

  switch (command) {
    case QUIT:
      // Send exit message to all of the worker threads
      for (int i = 0; i < arg_enclave.num_threads; i++) {
        print_info("Sending QUIT to all threads");

        sgx_thread_mutex_lock(&queue_mutex[i]);
        queue[i].push(new_job);
        sgx_thread_cond_signal(&job_cond[i]);
        sgx_thread_mutex_unlock(&queue_mutex[i]);
      }
      break;

    case SHARED:
    case EXCLUSIVE:
    case UNLOCK: {
      // Copy job parameters
      new_job.transaction_id = ((Job *)data)->transaction_id;
      new_job.row_id = ((Job *)data)->row_id;
      new_job.return_value = ((Job *)data)->return_value;
      new_job.finished = ((Job *)data)->finished;
      new_job.error = ((Job *)data)->error;

      // Send the requests to specific worker thread
      int thread_id = (int)((new_job.row_id % lockTableSize_) /
                            (lockTableSize_ / (arg_enclave.num_threads - 1)));
      sgx_thread_mutex_lock(&queue_mutex[thread_id]);
      queue[thread_id].push(new_job);
      sgx_thread_cond_signal(&job_cond[thread_id]);
      sgx_thread_mutex_unlock(&queue_mutex[thread_id]);
      break;
    }
    case REGISTER: {
      // Copy job parameters
      new_job.transaction_id = ((Job *)data)->transaction_id;
      new_job.finished = ((Job *)data)->finished;
      new_job.error = ((Job *)data)->error;

      // Send the requests to thread responsible for registering transactions
      sgx_thread_mutex_lock(&queue_mutex[arg_enclave.tx_thread_id]);
      queue[arg_enclave.tx_thread_id].push(new_job);
      sgx_thread_cond_signal(&job_cond[arg_enclave.tx_thread_id]);
      sgx_thread_mutex_unlock(&queue_mutex[arg_enclave.tx_thread_id]);
      break;
    }
    default:
      print_error("Received unknown command");
      break;
  }
}

void enclave_worker_thread() {
  sgx_thread_mutex_lock(&global_mutex);

  int thread_id = num;
  num += 1;

  sgx_thread_mutex_init(&queue_mutex[thread_id], NULL);
  sgx_thread_cond_init(&job_cond[thread_id], NULL);

  sgx_thread_mutex_unlock(&global_mutex);

  sgx_thread_mutex_lock(&queue_mutex[thread_id]);

  while (1) {
    print_info("Worker waiting for jobs");
    if (queue[thread_id].size() == 0) {
      sgx_thread_cond_wait(&job_cond[thread_id], &queue_mutex[thread_id]);
      continue;
    }

    print_info("Worker got a job");
    Job cur_job = queue[thread_id].front();
    Command command = cur_job.command;

    sgx_thread_mutex_unlock(&queue_mutex[thread_id]);

    switch (command) {
      case QUIT:
        sgx_thread_mutex_lock(&queue_mutex[thread_id]);
        queue[thread_id].pop();
        sgx_thread_mutex_unlock(&queue_mutex[thread_id]);
        sgx_thread_mutex_destroy(&queue_mutex[thread_id]);
        sgx_thread_cond_destroy(&job_cond[thread_id]);
        print_info("Enclave worker quitting");
        return;
      case SHARED:
      case EXCLUSIVE: {
        if (command == EXCLUSIVE) {
          print_info(
              ("(EXCLUSIVE) TXID: " + std::to_string(cur_job.transaction_id) +
               ", RID: " + std::to_string(cur_job.row_id))
                  .c_str());
        } else {
          print_info(
              ("(SHARED) TXID: " + std::to_string(cur_job.transaction_id) +
               ", RID: " + std::to_string(cur_job.row_id))
                  .c_str());
        }

        // Acquire lock and receive signature
        sgx_ec256_signature_t sig;
        int res = acquire_lock((void *)&sig, cur_job.transaction_id,
                               cur_job.row_id, command == EXCLUSIVE);

        if (res == SGX_ERROR_UNEXPECTED) {
          *cur_job.error = true;
        }

        *cur_job.finished = true;
        break;
      }
      case UNLOCK: {
        print_info(("(UNLOCK) TXID: " + std::to_string(cur_job.transaction_id) +
                    ", RID: " + std::to_string(cur_job.row_id))
                       .c_str());
        release_lock(cur_job.transaction_id, cur_job.row_id);
        break;
      }
      case REGISTER: {
        auto transactionId = cur_job.transaction_id;

        print_info(("Registering transaction " + std::to_string(transactionId))
                       .c_str());

        if (transactionTable_.find(transactionId) != transactionTable_.end()) {
          print_error("Transaction is already registered");
          *cur_job.error = true;
        } else {
          transactionTable_[transactionId] = new Transaction(transactionId);
        }
        *cur_job.finished = true;
        break;
      }
      default:
        print_error("Worker received unknown command");
    }

    sgx_thread_mutex_lock(&queue_mutex[thread_id]);
    queue[thread_id].pop();
  }

  return;
}

auto acquire_lock(void *signature, unsigned int transactionId,
                  unsigned int rowId, bool isExclusive) -> int {
  int ret;

  // Get the transaction object for the given transaction ID
  auto transaction = transactionTable_[transactionId];
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return SGX_ERROR_UNEXPECTED;
  }

  // Get the lock object for the given row ID
  auto lock = lockTable_[rowId];
  if (lock == nullptr) {
    lock = new Lock();
    lockTable_[rowId] = lock;
  }

  // Check if 2PL is violated
  if (transaction->getPhase() == Transaction::Phase::kShrinking) {
    print_error("Cannot acquire more locks according to 2PL");
    goto abort;
  }

  // Check for upgrade request
  if (transaction->hasLock(rowId) && isExclusive &&
      lock->getMode() == Lock::LockMode::kShared) {
    ret = lock->upgrade(transactionId);

    if (ret != SGX_SUCCESS) {
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

    if (ret != SGX_SUCCESS) {
      goto abort;
    }

    return ret;
  }

  print_error("Request for already acquired lock");

abort:
  abort_transaction(transaction);
  return SGX_ERROR_UNEXPECTED;
}

void release_lock(unsigned int transactionId, unsigned int rowId) {
  // Get the transaction object
  auto transaction = transactionTable_[transactionId];
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return;
  }

  // Get the lock object
  auto lock = lockTable_[rowId];
  if (lock == nullptr) {
    print_error("Lock does not exist");
    return;
  }

  transaction->releaseLock(rowId, lockTable_);
}

void abort_transaction(Transaction *transaction) {
  transactionTable_.erase(transaction->getTransactionId());
  transaction->releaseAllLocks(lockTable_);
  delete transaction;
}