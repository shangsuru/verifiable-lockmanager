#include "enclave.h"

Arg arg_enclave;  // configuration parameters for the enclave
int num = 0;      // global variable used to give every thread a unique ID
int transaction_count = 0;        // counts the number of active transactions
sgx_thread_mutex_t global_num_mutex;  // synchronizes access to num
sgx_thread_mutex_t *queue_mutex;  // synchronizes access to the job queue
sgx_thread_cond_t
    *job_cond;  // wakes up worker threads when a new job is available
std::vector<std::queue<Job>> queue;  // a job queue for each worker threads

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

      // If transaction is not registered, abort the request
      if (!contains(transactionTable_, new_job.transaction_id)) {
        print_error("Need to register transaction before lock requests");
        *new_job.error = true;
        *new_job.finished = true;
        return;
      }

      // Send the requests to specific worker thread
      int thread_id = (int)((new_job.row_id % lockTable_->size) /
                            (lockTable_->size / (arg_enclave.num_threads - 1)));
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
        bool ok = acquire_lock((void *)&sig, cur_job.transaction_id,
                               cur_job.row_id, command == EXCLUSIVE);
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
        auto lockBudget = cur_job.lock_budget;

        print_info(("Registering transaction " + std::to_string(transactionId))
                       .c_str());

        // TODO: Verify integrity of transaction table
        // TODO: for this to work, we need to work on our own local copy of the
        // hashtable

        auto transaction = (Transaction *)get(transactionTable_, transactionId);
        if (transaction == nullptr || transaction->transaction_id != 0) {
          // New transaction objects, that are created for new, not-yet
          // registered transaction beforehand by the untrusted application
          // always have their transaction ID set to -1 to differentiate them
          // from transaction objects refering to already registered
          // transactions
          print_error("Transaction is already registered");
          *cur_job.error = true;
        } else {
          transaction->transaction_id = transactionId;
          transaction->lock_budget = lockBudget;
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

auto get_block_timeout() -> int {
  print_warn("Lock timeout not yet implemented");
  // TODO: Implement getting the lock timeout
  return 0;
};

auto get_sealed_data_size() -> uint32_t {
  return sgx_calc_sealed_data_size((uint32_t)encoded_public_key.length(),
                                   sizeof(DataToSeal{}));
}

auto seal_keys(uint8_t *sealed_blob, uint32_t sealed_size) -> sgx_status_t {
  print_info("Sealing keys");
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  sgx_sealed_data_t *sealed_data = NULL;
  DataToSeal data;
  data.privateKey = ec256_private_key;
  data.publicKey = ec256_public_key;

  if (sealed_size != 0) {
    sealed_data = (sgx_sealed_data_t *)malloc(sealed_size);
    ret = sgx_seal_data((uint32_t)encoded_public_key.length(),
                        (uint8_t *)encoded_public_key.c_str(), sizeof(data),
                        (uint8_t *)&data, sealed_size, sealed_data);
    if (ret == SGX_SUCCESS)
      memcpy(sealed_blob, sealed_data, sealed_size);
    else
      free(sealed_data);
  }
  return ret;
}

auto unseal_keys(const uint8_t *sealed_blob, size_t sealed_size)
    -> sgx_status_t {
  print_info("Unsealing keys");
  sgx_status_t ret = SGX_ERROR_INVALID_PARAMETER;
  DataToSeal *unsealed_data = NULL;

  uint32_t dec_size = sgx_get_encrypt_txt_len((sgx_sealed_data_t *)sealed_blob);
  uint32_t mac_text_len =
      sgx_get_add_mac_txt_len((sgx_sealed_data_t *)sealed_blob);

  uint8_t *mac_text = (uint8_t *)malloc(mac_text_len);
  if (dec_size != 0) {
    unsealed_data = (DataToSeal *)malloc(dec_size);
    sgx_sealed_data_t *tmp = (sgx_sealed_data_t *)malloc(sealed_size);
    memcpy(tmp, sealed_blob, sealed_size);
    ret = sgx_unseal_data(tmp, mac_text, &mac_text_len,
                          (uint8_t *)unsealed_data, &dec_size);
    if (ret != SGX_SUCCESS) goto error;
    ec256_private_key = unsealed_data->privateKey;
    ec256_public_key = unsealed_data->publicKey;
  }

error:
  if (unsealed_data != NULL) free(unsealed_data);
  return ret;
}

auto generate_key_pair() -> int {
  print_info("Creating new key pair");
  if (context == NULL) sgx_ecc256_open_context(&context);
  sgx_status_t ret = sgx_ecc256_create_key_pair(&ec256_private_key,
                                                &ec256_public_key, context);
  encoded_public_key = base64_encode((unsigned char *)&ec256_public_key,
                                     sizeof(ec256_public_key));

  // append the number of characters for the encoded public key for easy
  // extraction from sealed text file
  encoded_public_key += std::to_string(encoded_public_key.length());
  return ret;
}

// Verifies a given message with its signature object and returns on success
// SGX_EC_VALID or on failure SGX_EC_INVALID_SIGNATURE
auto verify(const char *message, void *signature, size_t sig_len) -> int {
  sgx_ecc_state_handle_t context = NULL;
  sgx_ecc256_open_context(&context);
  uint8_t res;
  sgx_ec256_signature_t *sig = (sgx_ec256_signature_t *)signature;
  sgx_status_t ret = sgx_ecdsa_verify((uint8_t *)message,
                                      strnlen(message, MAX_SIGNATURE_LENGTH),
                                      &ec256_public_key, sig, &res, context);
  return res;
}

// Closes the ECDSA context
auto ecdsa_close() -> int {
  if (context == NULL) sgx_ecc256_open_context(&context);
  return sgx_ecc256_close_context(context);
}

auto acquire_lock(void *signature, int transactionId, int rowId,
                  bool isExclusive) -> bool {
  bool ok;

  // Get the transaction object for the given transaction ID
  auto transaction = (Transaction *)get(transactionTable_, transactionId);
  if (transaction == nullptr || transaction->transaction_id == 0) {
    print_error("Transaction was not registered");
    return false;
  }

  // Get the lock object for the given row ID
  auto lock = (Lock *)get(lockTable_, rowId);
  if (lock == nullptr) {
    return false;  // lock should have been allocated in untrusted app
  }

  // Check if 2PL is violated
  if (!transaction->growing_phase) {
    print_error("Cannot acquire more locks according to 2PL");
    goto abort;
  }

  // Check if lock budget is enough
  if (transaction->lock_budget < 1) {
    print_error("Lock budget exhausted");
    goto abort;
  }

  // Check for upgrade request
  if (hasLock(transaction, rowId) && isExclusive && !lock->exclusive) {
    ok = upgrade(lock, transactionId);

    if (!ok) {
      goto abort;
    }
    goto sign;
  }

  // Acquire lock in requested mode (shared, exclusive)
  if (!hasLock(transaction, rowId)) {
    if (addLock(transaction, rowId, isExclusive, lock)) {
      goto sign;
    }
    goto abort;
  }

  print_error("Request for already acquired lock");

abort:
  abort_transaction(transaction);
  return false;

sign:
  std::string string_to_sign =
      lock_to_string(transactionId, rowId, lock->exclusive);

  sgx_ecc_state_handle_t context = NULL;
  sgx_ecc256_open_context(&context);
  sgx_ecdsa_sign((uint8_t *)string_to_sign.c_str(),
                 strnlen(string_to_sign.c_str(), MAX_SIGNATURE_LENGTH),
                 &ec256_private_key, (sgx_ec256_signature_t *)signature,
                 context);

  return true;
}

void release_lock(int transactionId, int rowId) {
  // Get the transaction object
  auto transaction = (Transaction *)get(transactionTable_, transactionId);
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return;
  }

  // Get the lock object
  auto lock = (Lock *)get(lockTable_, rowId);
  if (lock == nullptr) {
    print_error("Lock does not exist");
    return;
  }

  releaseLock(transaction, rowId, lockTable_);
}

void abort_transaction(Transaction *transaction) {
  transaction_count--;
  remove(transactionTable_, transaction->transaction_id);
  releaseAllLocks(transaction, lockTable_);
}

auto verify_signature(char *signature, int transactionId, int rowId,
                      int isExclusive) -> int {
  std::string plain = lock_to_string(transactionId, rowId, isExclusive);

  std::string signature_string(signature);
  std::string x = signature_string.substr(0, signature_string.find("-"));
  std::string y = signature_string.substr(signature_string.find("-") + 1,
                                          signature_string.length());
  sgx_ec256_signature_t sig_struct;
  memcpy(sig_struct.x, base64_decode(x).c_str(), 8 * sizeof(uint32_t));
  memcpy(sig_struct.y, base64_decode(y).c_str(), 8 * sizeof(uint32_t));

  int ret =
      verify(plain.c_str(), (void *)&sig_struct, sizeof(sgx_ec256_signature_t));
  if (ret != SGX_SUCCESS) {
    print_error("Failed to verify signature");
  } else {
    print_info("Signature successfully verified");
  }
  return ret;
}

auto lock_to_string(int transactionId, int rowId, bool isExclusive)
    -> std::string {
  unsigned int block_timeout = get_block_timeout();

  std::string mode;
  if (isExclusive) {
    mode = "X";
  } else {
    mode = "S";
  }

  return std::to_string(transactionId) + "_" + std::to_string(rowId) + "_" +
         mode + "_" + std::to_string(block_timeout);
}