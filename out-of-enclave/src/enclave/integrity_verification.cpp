#include "integrity_verification.h"

const int maxEntriesPerLocktableBucket = 70;
int sizeOfSerializedLockEntry =
    3 +  // lock.key, lock.exclusive, lock.num_owners (compare lock struct)
    kTransactionBudget;  // owners of the lock can be at most kTransactionBudget
const int serializedLockBucketSize = 70 * sizeOfSerializedLockEntry;
uint32_t *serializedLockBucket = new uint32_t[serializedLockBucketSize];

auto hash_locktable_bucket(uint32_t *bucket, int numEntries)
    -> sgx_sha256_hash_t * {
  if (numEntries == 0) {
    return nullptr;
  }
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
  free(lockTableIntegrityHashes[key]);
  lockTableIntegrityHashes[key] = hash_locktable_bucket(bucket, numEntries);
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

  return ret;
}

auto release_lock_trusted(Transaction *transaction, int rowId, uint32_t *bucket,
                          int numEntries) -> bool {
  bool wasOwner = false;
  for (int i = 0; i < transaction->num_locked; i++) {
    if (transaction->locked_rows[i] == rowId) {
      wasOwner = true;
      break;
    }
  }

  if (wasOwner) {
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
  }
  return false;
}

auto verify_against_stored_hash(uint32_t *serialized, int numEntries,
                                sgx_sha256_hash_t *stored_hash) -> bool {
  sgx_sha256_hash_t *p_hash = hash_locktable_bucket(serialized, numEntries);

  bool equal = true;
  // If both are nullptr, they are equal
  if (!(p_hash == nullptr && stored_hash == nullptr)) {
    if (p_hash == nullptr || stored_hash == nullptr) {
      equal = false;  // if only one is nullptr they are unequal
    } else {          // none is nullptr: compare values
      for (int i = 0; i < SGX_SHA256_HASH_SIZE; i++) {
        auto a = (*p_hash)[i];
        auto b = (*stored_hash)[i];
        if (a != b) {
          equal = false;
        }
      }
    }
  }
  free(p_hash);
  return equal;
}