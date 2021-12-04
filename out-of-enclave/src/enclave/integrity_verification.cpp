#include "integrity_verification.h"

auto hash_locktable_bucket(Entry *bucket) -> sgx_sha256_hash_t * {
  if (bucket == nullptr) {
    return nullptr;
  }

  if (bucket->next == nullptr && ((Lock *)bucket->value)->num_owners == 0) {
    // Bucket only contains an unacquired lock
    return nullptr;
  }

  Entry *entry = bucket;
  const int numLockEntryFields = 3;
  uint32_t size = numLockEntryFields +
                  SGX_SHA256_HASH_SIZE;  // size of the serialized entry, see
                                         // locktabletable_entry_to_uint8_t
  uint8_t *data = new uint8_t[size];     // the serialized entry with lock
  sgx_status_t ret;                      // status code of sgx library calls

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
  const int numTransactionEntryFields = 6;
  uint32_t size =
      numTransactionEntryFields +
      SGX_SHA256_HASH_SIZE;  // size of the serialized entry, see function
                             // transactiontable_entry_to_uint8_t
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

auto integrity_verified_get_locktable(
    HashTable *&lockTable_,
    std::vector<sgx_sha256_hash_t *> &lockTableIntegrityHashes, int key)
    -> std::pair<Lock *, Entry *> {
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

  free(recomputed_hash);

  if (equal) {
    return std::make_pair((Lock *)get(copy, key), copy);
  }

  // Hashes are not the same
  free_lock_bucket_copy(copy);
  return std::make_pair(nullptr, nullptr);
}

auto integrity_verified_get_transactiontable(
    HashTable *&transactionTable_,
    std::vector<sgx_sha256_hash_t *> transactionTableIntegrityHashes, int key)
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
  free(recomputed_hash);

  if (equal) {
    return std::make_pair((Transaction *)get(copy, key), copy);
  }

  // Hashes are not the same
  if (copy != nullptr) {
    free_transaction_bucket_copy(copy);
  }
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
  free_transaction_copy(transaction);
  delete copy;
  if (nextEntry != nullptr) {
    free_transaction_bucket_copy(nextEntry);
  }
}

void update_integrity_hash_transactiontable(
    HashTable *&transactionTable_,
    std::vector<sgx_sha256_hash_t *> &transactionTableIntegrityHashes,
    Entry *entry) {
  sgx_sha256_hash_t *p_hash = hash_transactiontable_bucket(entry);

  // Free old hash
  free(transactionTableIntegrityHashes[hash(transactionTable_->size,
                                            entry->key)]);

  // Update hash
  transactionTableIntegrityHashes[hash(transactionTable_->size, entry->key)] =
      p_hash;
}

void update_integrity_hash_locktable(
    HashTable *&lockTable_,
    std::vector<sgx_sha256_hash_t *> &lockTableIntegrityHashes, Entry *entry) {
  sgx_sha256_hash_t *p_hash = hash_locktable_bucket(entry);

  // Free old hash
  free(lockTableIntegrityHashes[hash(lockTable_->size, entry->key)]);

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

  if (num_locked > 0) {
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
  } else {
    for (int i = 0; i < SGX_SHA256_HASH_SIZE; i++) {
      result[7 + i] = 0;
    }
  }
}