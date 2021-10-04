#include "enclave.h"

Arg arg_enclave;  // configuration parameters for the enclave
int num = 0;      // global variable used to give every thread a unique ID
int transaction_count = 0;  // counts the number of active transactions
sgx_thread_mutex_t global_num_mutex;  // synchronizes access to num
sgx_thread_mutex_t *queue_mutex;      // synchronizes access to the job queue
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

    print_info(("Worker " + std::to_string(thread_id) + " got a job").c_str());
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

        auto [transaction, entry] =
            integrity_verified_get_transactiontable(transactionId);
        if (entry == nullptr) {
          print_error(
              "Integrity verification failed on transaction table when "
              "registering");
          *cur_job.error = true;
        }
        if (transaction == nullptr || transaction->transaction_id != 0) {
          // New transaction objects, that are created for new, not-yet
          // registered transaction beforehand by the untrusted application
          // always have their transaction ID set to 0 to differentiate them
          // from already registered transactions
          print_error("Transaction is already registered");
          *cur_job.error = true;
        } else {
          // Register transaction by setting transaction ID and lock budget
          transaction->transaction_id = transactionId;
          transaction->lock_budget = lockBudget;
          transaction->aborted = false;
          update_integrity_hash_transactiontable(entry);
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

  // Append the number of characters for the encoded public key for easy
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

  // Get the transaction for the given transaction ID
  auto transactionUntrusted =
      (Transaction *)get(transactionTable_, transactionId);
  // Get a copy of the transaction in protected memory
  auto [transactionTrusted, transactionEntry] =
      integrity_verified_get_transactiontable(transactionId);

  if (transactionEntry == nullptr) {
    print_error(
        "Integrity verification failed on transaction table when acquiring a "
        "lock");
    return false;
  }
  // Run checks on trusted copy of the transaction, which is protected by the
  // enclave
  if (transactionTrusted == nullptr ||
      transactionTrusted->transaction_id == 0) {
    print_error("Transaction was not registered");
    return false;
  }

  // Get the lock object for the given row ID
  auto lockUntrusted = (Lock *)get(lockTable_, rowId);
  auto [lockTrusted, lockEntry] = integrity_verified_get_locktable(rowId);
  if (lockEntry == nullptr) {
    print_error(
        "Integrity verification failed on lock table when acquiring a lock");
    return false;
  }
  if (lockTrusted == nullptr) {
    print_error("Lock not allocated in untrusted app");
    return false;
  }

  // Check if 2PL is violated
  if (!transactionTrusted->growing_phase) {
    print_error("Cannot acquire more locks according to 2PL");
    goto abort;
  }

  // Check if lock budget is enough
  if (transactionTrusted->lock_budget < 1) {
    print_error("Lock budget exhausted");
    goto abort;
  }

  // Check for upgrade request
  if (hasLock(transactionTrusted, rowId) && isExclusive &&
      !lockTrusted->exclusive) {
    // Apply operation on temporary trusted copy
    ok = upgrade(lockTrusted, transactionId);

    // Update lock table hash
    update_integrity_hash_locktable(lockEntry);

    // Make changes in untrusted memory
    upgrade(lockUntrusted, transactionId);
    if (!ok) {
      goto abort;
    }
    goto sign;
  }

  // Acquire lock in requested mode (shared, exclusive)
  if (!hasLock(transactionTrusted, rowId)) {
    bool ok = addLock(transactionTrusted, rowId, isExclusive, lockTrusted);
    update_integrity_hash_transactiontable(transactionEntry);
    update_integrity_hash_locktable(lockEntry);

    // Repeat operation in untrusted part
    addLock(transactionUntrusted, rowId, isExclusive, lockUntrusted);

    if (ok) {
      goto sign;
    }
    goto abort;
  }

  print_error("Request for already acquired lock");

abort:
  // Abort transaction in both trusted and untrusted memory
  for (int i = 0; i < transactionTrusted->num_locked; i++) {
    int locked_row = transactionTrusted->locked_rows[i];
    auto [lockToReleaseTrusted, trustedBucket] =
        integrity_verified_get_locktable(locked_row);
    if (trustedBucket == nullptr) {
      print_error("Integrity verification failed");
    }
    auto lockToReleaseUntrusted = (Lock *)get(lockTable_, locked_row);

    release(lockToReleaseTrusted, transactionTrusted->transaction_id);
    release(lockToReleaseUntrusted, transactionTrusted->transaction_id);
    if (lockToReleaseUntrusted->num_owners == 0) {
      remove(lockTable_, locked_row);
    }
    update_integrity_hash_locktable(trustedBucket);
    free_lock_bucket_copy(trustedBucket);
  }
  transactionTrusted->transaction_id = 0;
  transactionTrusted->num_locked = 0;
  transactionTrusted->aborted = true;

  transactionUntrusted->transaction_id = 0;
  transactionUntrusted->num_locked = 0;
  transactionUntrusted->aborted = true;

  // Recompute the hash over the bucket in protected memory
  update_integrity_hash_transactiontable(transactionEntry);

  transaction_count--;
  remove(transactionTable_, transactionTrusted->transaction_id);

  free_lock_bucket_copy(lockEntry);
  free_transaction_bucket_copy(transactionEntry);
  return false;

sign:
  std::string string_to_sign =
      lock_to_string(transactionId, rowId, lockTrusted->exclusive);

  sgx_ecc_state_handle_t context = NULL;
  sgx_ecc256_open_context(&context);
  sgx_ecdsa_sign((uint8_t *)string_to_sign.c_str(),
                 strnlen(string_to_sign.c_str(), MAX_SIGNATURE_LENGTH),
                 &ec256_private_key, (sgx_ec256_signature_t *)signature,
                 context);

  free_lock_bucket_copy(lockEntry);
  free_transaction_bucket_copy(transactionEntry);
  return true;
}

void release_lock(int transactionId, int rowId) {
  // Get the transaction from untrusted memory and a trusted copy
  auto [transactionTrusted, transactionEntry] =
      integrity_verified_get_transactiontable(transactionId);
  auto transactionUntrusted =
      (Transaction *)get(transactionTable_, transactionId);

  if (transactionTrusted == nullptr) {
    print_error("Transaction was not registered");
    return;
  }

  // Get the lock from untrusted memory and a trusted copy
  auto [lockTrusted, lockEntry] = integrity_verified_get_locktable(rowId);
  auto lockUntrusted = (Lock *)get(lockTable_, rowId);

  if (lockTrusted == nullptr) {
    print_error("Lock does not exist");
    return;
  }

  // Release lock in trusted memory and update integrity hash
  HashTable *trustedLockTable = newHashTable(lockTable_->size);
  trustedLockTable->table[rowId] = lockEntry;
  releaseLock(transactionTrusted, rowId, trustedLockTable);
  update_integrity_hash_transactiontable(transactionEntry);
  update_integrity_hash_locktable(lockEntry);

  // Repeat operation in untrusted memory
  releaseLock(transactionUntrusted, rowId, lockTable_);

  free_lock_bucket_copy(lockEntry);
  free_transaction_bucket_copy(transactionEntry);
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

auto hash_locktable_bucket(Entry *bucket) -> sgx_sha256_hash_t * {
  if (bucket == nullptr) {
    return nullptr;
  }

  if (bucket->next == nullptr && ((Lock *)bucket->value)->num_owners == 0) {
    // Bucket only contains an unacquired lock
    return nullptr;
  }

  Entry *entry = bucket;
  uint32_t size = 3 + SGX_SHA256_HASH_SIZE;  // size of the serialized entry
  uint8_t *data = new uint8_t[size];         // the serialized entry with lock
  sgx_status_t ret;                          // status code of sgx library calls

  // To hold the hash over the entries in the bucket
  sgx_sha256_hash_t *p_hash =
      (sgx_sha256_hash_t *)malloc(sizeof(sgx_sha256_hash_t));

  // Need to initialize a handle struct, when we incrementally build the hash
  sgx_sha_state_handle_t *p_sha_handle =
      (sgx_sha_state_handle_t *)malloc(sizeof(sgx_sha_state_handle_t));
  ret = sgx_sha256_init(p_sha_handle);
  if (ret != SGX_SUCCESS) {
    print_error("Initializing SHA handler failed");
  }

  do {  // Update the hash with each entry in the bucket
    // Only include locks with owners into the hash
    if (((Lock *)entry->value)->num_owners != 0) {
      locktable_entry_to_uint8_t(entry, data);
      ret = sgx_sha256_update(data, size, *p_sha_handle);
      if (ret != SGX_SUCCESS) {
        print_error("Updating lock table hash failed");
      }
    }
    entry = entry->next;
  } while (entry != nullptr);

  delete[] data;

  ret = sgx_sha256_get_hash(*p_sha_handle, p_hash);
  if (ret != SGX_SUCCESS) {
    print_error("Getting hash failed");
  }

  ret = sgx_sha256_close(*p_sha_handle);
  free(p_sha_handle);
  if (ret != SGX_SUCCESS) {
    print_error("Closing SHA handle failed");
  }

  return p_hash;
}

auto hash_transactiontable_bucket(Entry *bucket) -> sgx_sha256_hash_t * {
  if (bucket == nullptr ||
      ((Transaction *)bucket->value)->transaction_id == 0) {
    // Bucket is empty or contains only unregistered transaction
    return nullptr;
  }

  Entry *entry = bucket;
  uint32_t size = 6 + SGX_SHA256_HASH_SIZE;  // size of the serialized entry
  uint8_t *data = new uint8_t[size];  // the serialized entry with transaction
  sgx_status_t ret;                   // status code of sgx library calls

  // To hold the hash over the entries in the bucket
  sgx_sha256_hash_t *p_hash =
      (sgx_sha256_hash_t *)malloc(sizeof(sgx_sha256_hash_t));

  // Need to initialize a handle struct, when we incrementally build the hash
  sgx_sha_state_handle_t *p_sha_handle =
      (sgx_sha_state_handle_t *)malloc(sizeof(sgx_sha_state_handle_t));
  ret = sgx_sha256_init(p_sha_handle);
  if (ret != SGX_SUCCESS) {
    print_error("Initializing SHA handler failed");
  }

  do {  // Update the hash with each entry in the bucket
    // Only include registered transactions
    if (((Transaction *)entry->value)->transaction_id != 0) {
      transactiontable_entry_to_uint8_t(entry, data);
      ret = sgx_sha256_update(data, size, *p_sha_handle);
      if (ret != SGX_SUCCESS) {
        print_error("Updating transaction table hash failed");
      }
    }
    entry = entry->next;
  } while (entry != nullptr);

  delete[] data;
  ret = sgx_sha256_get_hash(*p_sha_handle, p_hash);
  if (ret != SGX_SUCCESS) {
    print_error("Getting hash failed");
  }

  ret = sgx_sha256_close(*p_sha_handle);
  if (ret != SGX_SUCCESS) {
    print_error("Closing SHA handle failed");
  }
  free(p_sha_handle);
  return p_hash;
}

auto integrity_verified_get_locktable(int key) -> std::pair<Lock *, Entry *> {
  int index = hash(lockTable_->size, key);
  Entry *entry = lockTable_->table[index];

  // Need to copy bucket from untrusted to protected memory to prevent malicious
  // modifications during integrity verification
  Entry *copy = copy_lock_bucket(entry);

  // Verify the integrity of the bucket
  sgx_sha256_hash_t *recomputed_hash = hash_locktable_bucket(copy);
  sgx_sha256_hash_t *saved_hash = lockTableIntegrityHashes[index];

  bool equal = true;
  // If both are nullptr, they are equal
  if (!(recomputed_hash == nullptr && saved_hash == nullptr)) {
    if (recomputed_hash == nullptr || saved_hash == nullptr) {
      equal = false;  // if only one is nullptr they are unequal
    } else {          // none is nullptr: compare values
      for (int i = 0; i < SGX_SHA256_HASH_SIZE; i++) {
        auto a = (*recomputed_hash)[i];
        auto b = (*saved_hash)[i];
        if (a != b) {
          equal = false;
        }
      }
    }
  }

  if (equal) {
    return std::make_pair((Lock *)get(copy, key), copy);
  }

  // Hashes are not the same
  free_lock_bucket_copy(copy);
  return std::make_pair(nullptr, nullptr);
}

auto integrity_verified_get_transactiontable(int key)
    -> std::pair<Transaction *, Entry *> {
  int index = hash(transactionTable_->size, key);
  Entry *entry = transactionTable_->table[index];

  // Need to copy bucket from untrusted to protected memory to prevent malicious
  // modifications during integrity verification
  Entry *copy = copy_transaction_bucket(entry);

  // Verify the integrity of the bucket by checking if recomputed and saved hash
  // are the same
  sgx_sha256_hash_t *recomputed_hash = hash_transactiontable_bucket(entry);
  sgx_sha256_hash_t *saved_hash = transactionTableIntegrityHashes[index];

  bool equal = true;
  // If both are nullptr, they are equal
  if (!(recomputed_hash == nullptr && saved_hash == nullptr)) {
    if (recomputed_hash == nullptr || saved_hash == nullptr) {
      equal = false;  // if only one is nullptr they are unequal
    } else {          // none is nullptr: compare values
      for (int i = 0; i < SGX_SHA256_HASH_SIZE; i++) {
        auto a = (*recomputed_hash)[i];
        auto b = (*saved_hash)[i];
        if (a != b) {
          equal = false;
        }
      }
    }
  }
  if (equal) {
    return std::make_pair((Transaction *)get(copy, key), copy);
  }

  // Hashes are not the same
  return std::make_pair(nullptr, nullptr);
}

auto copy_lock_bucket(Entry *entry) -> Entry * {
  if (entry == nullptr) {
    return nullptr;
  }

  Entry *copy = newEntry(entry->key, copy_lock((Lock *)entry->value));
  copy->next = copy_lock_bucket(entry->next);
  return copy;
}

void free_lock_bucket_copy(Entry *&copy) {
  auto nextEntry = copy->next;
  auto lock = (Lock *)copy->value;
  free_lock_copy(lock);
  delete copy;
  if (nextEntry != nullptr) {
    free_lock_bucket_copy(nextEntry);
  }
}

auto copy_transaction_bucket(Entry *entry) -> Entry * {
  if (entry == nullptr) {
    return nullptr;
  }

  Entry *copy =
      newEntry(entry->key, copy_transaction((Transaction *)entry->value));
  copy->next = copy_transaction_bucket(entry->next);
  return copy;
}

void free_transaction_bucket_copy(Entry *&copy) {
  auto nextEntry = copy->next;
  auto transaction = (Transaction *)copy->value;
  // free_transaction_copy(transaction);
  delete copy;
  if (nextEntry != nullptr) {
    free_transaction_bucket_copy(nextEntry);
  }
}

void update_integrity_hash_transactiontable(Entry *entry) {
  sgx_sha256_hash_t *p_hash = hash_transactiontable_bucket(entry);
  // Free old hash
  if (transactionTableIntegrityHashes[hash(transactionTable_->size,
                                           entry->key)] != nullptr) {
    free(transactionTableIntegrityHashes[hash(transactionTable_->size,
                                              entry->key)]);
  }
  // Update hash
  transactionTableIntegrityHashes[hash(transactionTable_->size, entry->key)] =
      p_hash;
}

void update_integrity_hash_locktable(Entry *entry) {
  sgx_sha256_hash_t *p_hash = hash_locktable_bucket(entry);
  // Free old hash
  if (lockTableIntegrityHashes[hash(lockTable_->size, entry->key)] != nullptr) {
    free(lockTableIntegrityHashes[hash(lockTable_->size, entry->key)]);
  }
  // Update hash
  lockTableIntegrityHashes[hash(lockTable_->size, entry->key)] = p_hash;
}

void locktable_entry_to_uint8_t(Entry *entry, uint8_t *&result) {
  Lock *lock = (Lock *)(entry->value);
  int num_owners = lock->num_owners;

  // Get the entry key and every member of the lock struct
  result[0] = entry->key;
  result[1] = lock->exclusive;
  result[2] = num_owners;

  sgx_sha256_hash_t *p_hash =
      (sgx_sha256_hash_t *)malloc(sizeof(sgx_sha256_hash_t));
  sgx_status_t ret =
      sgx_sha256_msg((uint8_t *)lock->owners, sizeof(int) * num_owners, p_hash);
  if (ret != SGX_SUCCESS) {
    print_error("Error when serializing lock");
  }

  for (int i = 0; i < SGX_SHA256_HASH_SIZE; i++) {
    result[3 + i] = (*p_hash)[i];
  }
  free(p_hash);
}

void transactiontable_entry_to_uint8_t(Entry *&entry, uint8_t *&result) {
  Transaction *transaction = (Transaction *)(entry->value);
  int num_locked = transaction->num_locked;

  // Get the entry key and every member of the transaction struct
  result[0] = entry->key;
  result[1] = transaction->transaction_id;
  result[2] = transaction->aborted;
  result[3] = transaction->growing_phase;
  result[4] = transaction->lock_budget;
  result[5] = transaction->locked_rows_size;
  result[6] = num_locked;

  sgx_sha256_hash_t *p_hash =
      (sgx_sha256_hash_t *)malloc(sizeof(sgx_sha256_hash_t));
  sgx_status_t ret = sgx_sha256_msg((uint8_t *)transaction->locked_rows,
                                    sizeof(int) * num_locked, p_hash);
  if (ret != SGX_SUCCESS) {
    print_error("Error when serializing transaction");
  }

  for (int i = 0; i < SGX_SHA256_HASH_SIZE; i++) {
    result[7 + i] = (*p_hash)[i];
  }
  free(p_hash);
}