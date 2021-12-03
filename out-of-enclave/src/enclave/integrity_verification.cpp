#include "integrity_verification.h"

const int maxEntriesPerLocktableBucket = 70;
int sizeOfSerializedLockEntry =
    3 +  // lock.key, lock.exclusive, lock.num_owners (compare lock struct)
    kTransactionBudget;  // owners of the lock can be at most kTransactionBudget
const int serializedLockBucketSize = 70 * sizeOfSerializedLockEntry;
uint32_t *serializedLockBucket = new uint32_t[serializedLockBucketSize];

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

auto hash_locktable_bucket(uint32_t *bucket, int numEntries)
    -> sgx_sha256_hash_t * {
  sgx_sha256_hash_t *p_hash =
      (sgx_sha256_hash_t *)malloc(sizeof(sgx_sha256_hash_t));
  // because the bucket contains "numEntries" entries of size
  // "sizeOfSerializedLockEntry", but is of type uint32_t, which is 4x the size
  // of uint8_t
  uint32_t src_len = numEntries * sizeOfSerializedLockEntry * 4;
  sgx_status_t ret = sgx_sha256_msg((uint8_t *)bucket, src_len, p_hash);
  return p_hash;
}

void update_integrity_hash_locktable(
    uint32_t *bucket, int key, int numEntries,
    std::vector<sgx_sha256_hash_t *> &lockTableIntegrityHashes) {
  // Free old hash
  free(lockTableIntegrityHashes[key]);

  // Update hash
  if (numEntries == 0) {
    lockTableIntegrityHashes[key] = nullptr;
  } else {
    sgx_sha256_hash_t *p_hash = hash_locktable_bucket(bucket, numEntries);
    lockTableIntegrityHashes[key] = p_hash;
  }
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

auto locktable_bucket_to_uint32_t(Entry *&bucket, int numEntries)
    -> uint32_t * {
  Entry *entry = bucket;
  Lock *lock;
  int num_owners;

  for (int i = 0; i < numEntries; i++) {
    lock = (Lock *)(entry->value);
    num_owners = lock->num_owners;
    serializedLockBucket[i * sizeOfSerializedLockEntry] = entry->key;
    serializedLockBucket[i * sizeOfSerializedLockEntry + 1] = lock->exclusive;
    serializedLockBucket[i * sizeOfSerializedLockEntry + 2] = num_owners;
    for (int j = 0; j < kTransactionBudget; j++) {
      if (j < num_owners)
        serializedLockBucket[i * sizeOfSerializedLockEntry + 3 + j] =
            lock->owners[j];
      else
        serializedLockBucket[i * sizeOfSerializedLockEntry + 3 + j] = 0;
    }

    entry = entry->next;
  }

  return serializedLockBucket;
}

auto add_lock_trusted(Transaction *transaction, int rowId, bool isExclusive,
                      uint32_t *bucket) -> bool {
  if (transaction->aborted) {
    return false;
  }

  bool ret;

  // Find the lock inside the serialized bucket
  int i = 0;  // start index of the serialized lock entry
  while (bucket[i] != rowId &&
         i < serializedLockBucketSize - sizeOfSerializedLockEntry) {
    // RID of the lock is at the beginning of each serialized lock entry
    i += sizeOfSerializedLockEntry;
  }

  //  Get exclusive access on the lock
  if (isExclusive) {
    int numOwners = bucket[i + 2];
    if (numOwners == 0) {
      bucket[i + 1] = true;                         // set lock exclusive
      bucket[i + 2] = 1;                            // set num_owners
      bucket[i + 3] = transaction->transaction_id;  // set owner
      ret = true;
    } else {
      ret = false;
    }
    //  Get shared access on the lock
  } else {
    bool lockExclusive = bucket[i + 1];
    if (!lockExclusive) {
      bucket[i + 2]++;  // increment num_owners
      int numOwners = bucket[i + 2];
      bucket[i + 2 + numOwners] = transaction->transaction_id;  // set new owner
      ret = true;
    } else {
      ret = false;
    }
  }

  if (ret) {
    if (transaction->num_locked == 0) {
      transaction->locked_rows = new int[1];
      transaction->locked_rows[0] = rowId;
    } else {
      transaction->locked_rows =
          (int *)realloc(transaction->locked_rows,
                         sizeof(int) * (transaction->num_locked + 1));
      transaction->locked_rows[transaction->num_locked] = rowId;
    }

    transaction->num_locked++;
    transaction->lock_budget--;
  }

  return ret;
}

auto release_lock_trusted(Transaction *transaction, int rowId, uint32_t *bucket,
                          int numEntries) -> bool {
  bool wasOwner = false;
  for (int i = 0; i < transaction->num_locked; i++) {
    if (transaction->locked_rows[i] == rowId) {
      for (int j = i; j < transaction->num_locked - 1; j++) {
        transaction->locked_rows[j] = transaction->locked_rows[j + 1];
      }
      wasOwner = true;
      break;
    }
  }

  if (wasOwner) {
    transaction->num_locked--;
    transaction->growing_phase = false;

    int transactionId = transaction->transaction_id;
    // Find the lock inside the serialized bucket
    int i = 0;  // start index of the serialized lock entry
    while (bucket[i] != rowId &&
           i < serializedLockBucketSize - sizeOfSerializedLockEntry) {
      // RID of the lock is at the beginning of each serialized lock entry
      i += sizeOfSerializedLockEntry;
    }

    int numOwners = bucket[i + 2];
    for (int j = 0; j < numOwners; j++) {
      if (bucket[i + 3 + j] == transactionId) {
        // Lock is owned by the given transaction
        for (int k = j; k < numOwners - 1; k++) {
          bucket[i + 3 + k] = bucket[i + 3 + k + 1];
        }
        bucket[i + 3 + numOwners - 1] = 0;
        bucket[i + 1] = false;  // not exclusive anymore
        bucket[i + 2]--;        // decrement num_owners
        break;
      }
    }

    if (bucket[i + 2] == 0) {  // unowned lock
      // Remove the lock
      for (int j = i; j < sizeOfSerializedLockEntry * (numEntries - 1); j++) {
        bucket[i + j] = bucket[i + j + sizeOfSerializedLockEntry];
      }
      for (int j = 0; j < sizeOfSerializedLockEntry; j++) {
        bucket[sizeOfSerializedLockEntry * (numEntries - 1) + j] = 0;
      }
      return true;
    }
    return false;
  }
}