#include "enclave.h"

Arg arg_enclave;  // configuration parameters for the enclave
int num = 0;      // global variable used to give every thread a unique ID
int transaction_count = 0;  // counts the number of active transactions
sgx_thread_mutex_t global_num_mutex;  // synchronizes access to num
sgx_thread_mutex_t *queue_mutex;      // synchronizes access to the job queue
sgx_thread_cond_t
    *job_cond;  // wakes up worker threads when a new job is available
std::vector<std::queue<Job>> queue;  // a job queue for each worker threads
sgx_ecc_state_handle_t *contexts;    // context for signing for each thread

void enclave_init_values(Arg arg, HashTable *lock_table,
                         HashTable *transaction_table) {
  // Get configuration parameters
  arg_enclave = arg;
  lockTable_ = lock_table;
  transactionTable_ = transaction_table;

  // Initialize mutex variables
  sgx_thread_mutex_init(&global_num_mutex, NULL);
  queue_mutex = (sgx_thread_mutex_t *)malloc(sizeof(sgx_thread_mutex_t) *
                                             arg_enclave.num_threads);
  job_cond = (sgx_thread_cond_t *)malloc(sizeof(sgx_thread_cond_t) *
                                         arg_enclave.num_threads);

  // Initialize job queues
  contexts = (sgx_ecc_state_handle_t *)malloc(arg_enclave.num_threads *
                                              sizeof(sgx_ecc_state_handle_t));
  for (int i = 0; i < arg_enclave.num_threads; i++) {
    queue.push_back(std::queue<Job>());
    sgx_ecc256_open_context(&contexts[i]);
  }

  // Allocate space for one hash per bucket
  transactionTableIntegrityHashes.resize(transactionTable_->size);
  lockTableIntegrityHashes.resize(lockTable_->size);
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
      new_job.wait_for_result = ((Job *)data)->wait_for_result;

      if (new_job.wait_for_result) {
        new_job.return_value = ((Job *)data)->return_value;
        new_job.finished = ((Job *)data)->finished;
        new_job.error = ((Job *)data)->error;
      }

      // If transaction is not registered, abort the request
      if (!contains(transactionTable_, new_job.transaction_id)) {
        print_error("Need to register transaction before lock requests");
        if (new_job.wait_for_result) {
          *new_job.error = true;
          *new_job.finished = true;
        }
        return;
      }

      // Send the requests to specific worker thread
      int thread_id =
          (int)((new_job.row_id % lockTable_->size) /
                ((float)lockTable_->size / (arg_enclave.num_threads - 1)));
      sgx_thread_mutex_lock(&queue_mutex[thread_id]);
      queue[thread_id].push(new_job);
      sgx_thread_cond_signal(&job_cond[thread_id]);
      sgx_thread_mutex_unlock(&queue_mutex[thread_id]);
      break;
    }
    case REGISTER: {
      // Copy job parameters
      new_job.transaction_id = ((Job *)data)->transaction_id;
      new_job.lock_budget = ((Job *)data)->lock_budget;
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

void enclave_process_request() {
  sgx_thread_mutex_lock(&global_num_mutex);

  int thread_id = num;
  num += 1;

  sgx_thread_mutex_init(&queue_mutex[thread_id], NULL);
  sgx_thread_cond_init(&job_cond[thread_id], NULL);

  sgx_thread_mutex_unlock(&global_num_mutex);

  sgx_thread_mutex_lock(&queue_mutex[thread_id]);

  while (1) {
    print_info("Worker waiting for jobs");
    if (queue[thread_id].size() == 0) {
      sgx_thread_cond_wait(&job_cond[thread_id], &queue_mutex[thread_id]);
      continue;
    }

    auto log = ("Worker " + std::to_string(thread_id) + " got a job").c_str();
    print_info(log);
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
        sgx_ecc256_close_context(contexts[thread_id]);
        print_info("Enclave worker quitting");
        return;
      case SHARED:
      case EXCLUSIVE: {
        if (command == EXCLUSIVE) {
          auto log =
              ("(EXCLUSIVE) TXID: " + std::to_string(cur_job.transaction_id) +
               ", RID: " + std::to_string(cur_job.row_id))
                  .c_str();
          print_info(log);
        } else {
          auto log =
              ("(SHARED) TXID: " + std::to_string(cur_job.transaction_id) +
               ", RID: " + std::to_string(cur_job.row_id))
                  .c_str();
          print_info(log);
        }

        // Acquire lock and receive signature
        sgx_ec256_signature_t sig;
        bool ok = acquire_lock((void *)&sig, cur_job.transaction_id,
                               cur_job.row_id, command == EXCLUSIVE, thread_id);
        if (cur_job.wait_for_result) {
          if (!ok) {
            *cur_job.error = true;
          } else {
            // Write base64 encoded signature into the return value of the job
            // struct
            std::string encoded_signature =
                base64_encode((unsigned char *)sig.x, sizeof(sig.x)) + "-" +
                base64_encode((unsigned char *)sig.y, sizeof(sig.y));

            volatile char *p = cur_job.return_value;
            size_t signature_size = 89;
            for (int i = 0; i < signature_size; i++) {
              *p++ = encoded_signature.c_str()[i];
            }
          }
          *cur_job.finished = true;
        }
        break;
      }
      case UNLOCK: {
        auto log = ("(UNLOCK) TXID: " + std::to_string(cur_job.transaction_id) +
                    ", RID: " + std::to_string(cur_job.row_id))
                       .c_str();
        print_info(log);
        if (cur_job.wait_for_result) {
          *cur_job.finished = true;
        }
        break;
      }
      case REGISTER: {
        auto transactionId = cur_job.transaction_id;
        auto lockBudget = cur_job.lock_budget;

        auto log = ("Registering transaction " + std::to_string(transactionId))
                       .c_str();
        print_info(log);

        auto [transaction, entry] = integrity_verified_get_transactiontable(
            transactionTable_, transactionTableIntegrityHashes, transactionId);
        if (entry == nullptr) {
          print_error(
              "Integrity verification failed on transaction "
              "table when "
              "registering");
          *cur_job.error = true;
        }
        if (transaction == nullptr || transaction->transaction_id != 0) {
          // New transaction objects, that are created for new,
          // not-yet registered transaction beforehand by the
          // untrusted application always have their transaction
          // ID set to 0 to differentiate them from already
          // registered transactions
          print_error("Transaction is already registered");
          *cur_job.error = true;
        } else {
          // Register transaction by setting transaction ID and
          // lock budget
          transaction->transaction_id = transactionId;
          transaction->lock_budget = lockBudget;
          transaction->aborted = false;
          update_integrity_hash_transactiontable(
              transactionTable_, transactionTableIntegrityHashes, entry);
          free_transaction_bucket_copy(entry);

          // Make changes in untrusted memory as well
          auto transactionUntrusted =
              (Transaction *)get(transactionTable_, transactionId);
          transactionUntrusted->transaction_id = transactionId;
          transactionUntrusted->lock_budget = lockBudget;
          transactionUntrusted->aborted = false;
        }
        *cur_job.finished = true;
        transaction_count++;
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

auto acquire_lock(void *signature, int transactionId, int rowId,
                  bool isExclusive, int threadId) -> bool {
  bool ok;
  auto transactionUntrusted =
      (Transaction *)get(transactionTable_, transactionId);
  auto lockUntrusted = (Lock *)get(lockTable_, rowId);
  addLock(transactionUntrusted, rowId, isExclusive, lockUntrusted);

  // Sign the lock
  std::string string_to_sign =
      lock_to_string(transactionId, rowId, isExclusive);

  sgx_ecdsa_sign((uint8_t *)string_to_sign.c_str(),
                 strnlen(string_to_sign.c_str(), MAX_SIGNATURE_LENGTH),
                 &ec256_private_key, (sgx_ec256_signature_t *)signature,
                 contexts[threadId]);

  // free_transaction_bucket_copy(transactionEntry);
  return true;
}