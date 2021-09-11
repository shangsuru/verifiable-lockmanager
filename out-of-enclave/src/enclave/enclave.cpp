#include "enclave.h"

int num = 0;
int exit_count = 0;

hashtable *ht_enclave = NULL;
MACbuffer *MACbuf_enclave = NULL;
BucketMAC *MACTable = NULL;
Arg arg_enclave;
int ratio_root_per_buckets = 0;

sgx_thread_mutex_t global_mutex;
sgx_thread_mutex_t *queue_mutex;
sgx_thread_cond_t *job_cond;
std::vector<std::queue<job *> > queue;

uint8_t key_hash_func(char *key) {
  unsigned long int hashval = 7;
  int i = 0;
  /* Convert our string to an integer */
  while (hashval < ULONG_MAX && i < strlen(key)) {
    hashval = hashval * 11;
    hashval += key[i];
    i++;
  }

  return hashval % 256;  // int8_t can store 0 ~ 255
}

int ht_hash(int key) { return key % ht_enclave->size; }
/* Hash a string for a particular hash table. */
int ht_hash(char *key) {
  unsigned long int hashval = 7;
  int i = 0;
  /* Convert our string to an integer */
  while (hashval < ULONG_MAX && i < strlen(key)) {
    hashval = hashval * 61;
    hashval += key[i];
    i++;
  }

  return hashval % ht_enclave->size;
}

/* Retrieve a key-value pair from a hash table. */
entry *ht_get_o(char *key, int32_t key_size, char **plain_key_val, int *kv_pos,
                uint8_t (&updated_nac)[NAC_SIZE]) {
  int bin = 0;
  uint8_t key_hash = 0;
  entry *pair;

  bin = ht_hash(key);
  *kv_pos = 0;

  if (arg_enclave.key_opt) {
    key_hash = key_hash_func(key);
  }

  /* Step through the bin, looking for our value. */
  pair = ht_enclave->table[bin];

  if (arg_enclave.key_opt) {
    while (pair != NULL) {
      // check key_size && key_hash value to find matching key
      if (pair->key_val != NULL && pair->key_size == key_size &&
          key_hash == pair->key_hash) {
        if ((*plain_key_val = decrypt_key_val_and_compare(
                 key, pair->key_val, pair->key_size, pair->val_size, pair->nac,
                 updated_nac)) != NULL) {
          assert(plain_key_val != NULL);

          /** range check operations **/
          assert(sgx_is_outside_enclave(pair, sizeof(*pair)));
          assert(sgx_is_outside_enclave(pair->key_val,
                                        pair->key_size + pair->val_size));

          return pair;
        }
      }
      pair = pair->next;
      (*kv_pos)++;
    }

    return NULL;
  }

  /** When the key optimization is on, ShieldStore runs two-step search **/
  *kv_pos = 0;
  pair = ht_enclave->table[bin];

  while (pair != NULL) {
    // check key_size && key_hash value to find matching key
    if (pair->key_val != NULL && pair->key_size == key_size) {
      if ((*plain_key_val = decrypt_key_val_and_compare(
               key, pair->key_val, pair->key_size, pair->val_size, pair->nac,
               updated_nac)) != NULL) {
        assert(plain_key_val != NULL);

        /** range check operations **/
        assert(sgx_is_outside_enclave(pair, sizeof(*pair)));
        assert(sgx_is_outside_enclave(pair->key_val,
                                      pair->key_size + pair->val_size));

        return pair;
      }
    }
    pair = pair->next;
    (*kv_pos)++;
  }

  return NULL;
}

/* Create a key-value pair. */
entry *ht_newpair(int key_size, int val_size, uint8_t key_hash, char *key_val,
                  uint8_t *nac, uint8_t *mac) {
  entry *newpair;

  if ((newpair = (entry *)malloc(sizeof(entry))) == NULL) return NULL;

  if ((newpair->key_val =
           (char *)malloc(sizeof(char) * (key_size + val_size))) == NULL)
    return NULL;

  if (memcpy(newpair->key_val, key_val, key_size + val_size) == NULL)
    return NULL;

  if (memcpy(newpair->nac, nac, NAC_SIZE) == NULL) return NULL;

  if (memcpy(newpair->mac, mac, MAC_SIZE) == NULL) return NULL;

  newpair->key_hash = key_hash;
  newpair->key_size = key_size;
  newpair->val_size = val_size;
  newpair->next = NULL;

  return newpair;
}

/* Insert a key-value pair into a hash table. */
void ht_set_o(entry *updated_entry, char *key, char *key_val, uint8_t *nac,
              uint8_t *mac, uint32_t key_len, uint32_t val_len, int kv_pos) {
  int bin = 0;
  uint8_t key_hash = 0;
  int pos = -1;

  entry *newpair = NULL;

  bin = ht_hash(key);
  key_hash = key_hash_func(key);

  // Update
  if (updated_entry != NULL) {
    if (updated_entry->val_size != val_len)
      updated_entry->key_val = (char *)realloc(
          updated_entry->key_val, sizeof(char) * (key_len + val_len));
    memcpy(updated_entry->key_val, key_val, key_len + val_len);
    memcpy(updated_entry->nac, nac, NAC_SIZE);
    memcpy(updated_entry->mac, mac, MAC_SIZE);
    updated_entry->val_size = val_len;

    if (arg_enclave.mac_opt) {
      /** The order of MAC bucket is reversed to hash chain order **/
      pos = MACbuf_enclave->entry[bin].size - kv_pos - 1;
      memcpy(MACbuf_enclave->entry[bin].mac + (MAC_SIZE * pos), mac, MAC_SIZE);
    }
  }
  // Insert
  else {
    newpair = ht_newpair(key_len, val_len, key_hash, key_val, nac, mac);

    if (!newpair) print_error("new pair problem");

    /** New-pair should be inserted into the first entry of the bucket chain **/
    if (updated_entry == ht_enclave->table[bin]) {
      newpair->next = NULL;
      ht_enclave->table[bin] = newpair;
    } else if (!updated_entry) {
      newpair->next = ht_enclave->table[bin];
      ht_enclave->table[bin] = newpair;
    } else {
      print_error("do not enter here");
    }

    if (arg_enclave.mac_opt) {
      // mac buffer increase
      MACbuf_enclave->entry[bin].size++;
      /** The order of MAC bucket is reversed to hash chain order **/
      memcpy(MACbuf_enclave->entry[bin].mac + (MAC_SIZE * kv_pos), mac,
             MAC_SIZE);
    }
  }
}

/* append the value of key-value pair */
void ht_append_o(entry *updated_entry, char *key, char *key_val, uint8_t *nac,
                 uint8_t *mac, uint32_t key_len, uint32_t val_len, int kv_pos) {
  int bin = 0;

  bin = ht_hash(key);

  // Update
  if (updated_entry != NULL) {
    updated_entry->key_val = (char *)realloc(
        updated_entry->key_val, sizeof(char) * (key_len + val_len));
    memcpy(updated_entry->key_val, key_val, key_len + val_len);
    memcpy(updated_entry->nac, nac, NAC_SIZE);
    memcpy(updated_entry->mac, mac, MAC_SIZE);
    updated_entry->val_size = val_len;
  } else {
    print_warn("There's no data in database");
    return;
  }

  if (arg_enclave.mac_opt) {
    /** The order of MAC bucket is reversed to hash chain order **/
    memcpy(MACbuf_enclave->entry[bin].mac + (MAC_SIZE * kv_pos), mac, MAC_SIZE);
  }
}

/* Global Symmetric Key */
const sgx_ec_key_128bit_t gsk = {0x72, 0x12, 0x8a, 0x7a, 0x17, 0x52,
                                 0x6e, 0xbf, 0x85, 0xd0, 0x3a, 0x62,
                                 0x37, 0x30, 0xae, 0xad};

/**
 * decrypt key on the hash structure and compare to matching key
 */
char *decrypt_key_val_and_compare(char *key, char *cipher, uint32_t key_len,
                                  uint32_t val_len, uint8_t *nac,
                                  uint8_t *updated_nac) {
  uint8_t *temp_plain = NULL;
  uint8_t temp_nac[NAC_SIZE];
  sgx_status_t ret = SGX_SUCCESS;

  memcpy(temp_nac, nac, NAC_SIZE);

  temp_plain = (uint8_t *)malloc(sizeof(uint8_t) * (key_len + val_len));

  ret = sgx_aes_ctr_decrypt(&gsk, (uint8_t *)cipher, key_len + val_len,
                            temp_nac, NAC_SIZE, temp_plain);

  if (ret != SGX_SUCCESS) {
    assert(0);
  }

  if (memcmp(temp_plain, key, key_len) == 0) {
    memcpy(updated_nac, temp_nac, NAC_SIZE);
  } else {
    free(temp_plain);
    temp_plain = NULL;
  }
  return (char *)temp_plain;
}

/**
 * Decrypt the encrypted key_val and get the key
 * TODO: it doesn't update the IV/counter value(nac)
 **/
char *decrypt_and_get_key(char *cipher, uint32_t key_len, uint32_t val_len,
                          uint8_t *nac) {
  uint8_t *temp_plain = NULL;
  uint8_t *temp_key = NULL;

  // Do not update the IV/counter
  uint8_t temp_nac[NAC_SIZE];
  sgx_status_t ret = SGX_SUCCESS;

  memcpy(temp_nac, nac, NAC_SIZE);

  temp_plain = (uint8_t *)malloc(sizeof(uint8_t) * (key_len + val_len));
  temp_key = (uint8_t *)malloc(sizeof(uint8_t) * (key_len));

  ret = sgx_aes_ctr_decrypt(&gsk, (uint8_t *)cipher, key_len + val_len,
                            temp_nac, NAC_SIZE, temp_plain);

  if (ret != SGX_SUCCESS) {
    assert(0);
  }

  memcpy(temp_key, temp_plain, key_len);
  free(temp_plain);

  return (char *)temp_key;
}

/**
 * get all the mac entry for a same hash bucket
 * for integrity verification
 **/
void get_chain_mac(int hash_val, uint8_t *mac) {
  uint8_t temp_mac[MAC_SIZE];
  uint8_t *aggregate_mac;
  entry *pair;

  int count = 0;
  int i;
  int aggregate_mac_idx = 0;
  int index;
  sgx_status_t ret = SGX_SUCCESS;

  memset(temp_mac, 0, MAC_SIZE);

  /** bucket start index for verifying integrity **/
  int start_index =
      (int)(hash_val / ratio_root_per_buckets) * ratio_root_per_buckets;

  if (arg_enclave.mac_opt) {
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      count += MACbuf_enclave->entry[index].size;
    }
    aggregate_mac = (uint8_t *)malloc(MAC_SIZE * count);
    memset(aggregate_mac, 0, MAC_SIZE * count);

    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      memcpy(aggregate_mac + aggregate_mac_idx,
             MACbuf_enclave->entry[index].mac,
             MAC_SIZE * MACbuf_enclave->entry[index].size);
      aggregate_mac_idx += (MAC_SIZE * MACbuf_enclave->entry[index].size);
    }
  } else {
    /* Check chaining size */
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      pair = ht_enclave->table[index];
      while (pair != NULL) {
        count++;
        pair = pair->next;
      }
    }

    // verify
    aggregate_mac = (uint8_t *)malloc(MAC_SIZE * count);
    memset(aggregate_mac, 0, MAC_SIZE * count);

    i = 0;
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      pair = ht_enclave->table[index];
      while (pair != NULL) {
        memcpy(aggregate_mac + (MAC_SIZE * i), pair->mac, MAC_SIZE);
        i++;
        pair = pair->next;
      }
    }
  }

  ret = sgx_rijndael128_cmac_msg(&gsk, aggregate_mac, MAC_SIZE * count,
                                 &temp_mac);

  if (ret != SGX_SUCCESS) {
    assert(0);
  }
  free(aggregate_mac);

  /* Copy generated MAC to enclave */
  memcpy(mac, (char *)temp_mac, MAC_SIZE);
}

/**
 * verify integrity tree &
 * update integrity tree with updated hash values
 **/
sgx_status_t enclave_rebuild_tree_root(int hash_val, int kv_pos, bool is_insert,
                                       uint8_t *prev_mac) {
  uint8_t temp_mac[MAC_SIZE];
  uint8_t *aggregate_mac;
  uint8_t *prev_aggregate_mac;
  entry *pair;

  int total_mac_count = 0, prev_mac_count = 0, cur_mac_count = 0;
  int aggregate_mac_idx = 0;
  int i, index, updated_idx = -1;
  bool is_cur_idx = false;

  sgx_status_t ret = SGX_SUCCESS;

  memset(temp_mac, 0, MAC_SIZE);

  /** bucket start index for verifying integrity **/
  int start_index =
      (int)(hash_val / ratio_root_per_buckets) * ratio_root_per_buckets;

  if (arg_enclave.mac_opt) {
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      if (index == hash_val) {
        prev_mac_count = total_mac_count;
        /** The location of MAC is the reverse order over the location of key
         * value **/
        if (is_insert)
          updated_idx = prev_mac_count + kv_pos;
        else
          updated_idx =
              prev_mac_count + (MACbuf_enclave->entry[index].size - kv_pos - 1);
      }
      total_mac_count += MACbuf_enclave->entry[index].size;
    }

    aggregate_mac = (uint8_t *)malloc(MAC_SIZE * total_mac_count);
    memset(aggregate_mac, 0, MAC_SIZE * total_mac_count);
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      memcpy(aggregate_mac + aggregate_mac_idx,
             MACbuf_enclave->entry[index].mac,
             MAC_SIZE * MACbuf_enclave->entry[index].size);
      aggregate_mac_idx += (MAC_SIZE * MACbuf_enclave->entry[index].size);
    }
  } else {
    /* Check chaining size */
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      pair = ht_enclave->table[index];
      is_cur_idx = false;
      if (index == hash_val) {
        prev_mac_count = total_mac_count;
        is_cur_idx = true;
      }
      while (pair != NULL) {
        if (is_cur_idx) cur_mac_count++;
        total_mac_count++;
        pair = pair->next;
      }
    }
    if (is_insert)
      updated_idx = prev_mac_count + (cur_mac_count - kv_pos - 1);
    else
      updated_idx = prev_mac_count + kv_pos;

    // verify
    aggregate_mac = (uint8_t *)malloc(MAC_SIZE * total_mac_count);
    memset(aggregate_mac, 0, MAC_SIZE * total_mac_count);

    i = 0;
    for (index = start_index; index < start_index + ratio_root_per_buckets;
         index++) {
      pair = ht_enclave->table[index];
      while (pair != NULL) {
        memcpy(aggregate_mac + (MAC_SIZE * i), pair->mac, MAC_SIZE);
        i++;
        pair = pair->next;
      }
    }
  }

  // generate previous mac
  if (is_insert) {
    assert(kv_pos == 0);

    if (total_mac_count > 1) {
      prev_aggregate_mac = (uint8_t *)malloc(MAC_SIZE * (total_mac_count - 1));
      memset(prev_aggregate_mac, 0, MAC_SIZE * (total_mac_count - 1));

      memcpy(prev_aggregate_mac, aggregate_mac, updated_idx * MAC_SIZE);
      memcpy(prev_aggregate_mac + (updated_idx * MAC_SIZE),
             aggregate_mac + (updated_idx + 1) * MAC_SIZE,
             (total_mac_count - updated_idx - 1) * MAC_SIZE);

      // verify tree root using previous mac
      ret =
          sgx_rijndael128_cmac_msg(&gsk, prev_aggregate_mac,
                                   MAC_SIZE * (total_mac_count - 1), &temp_mac);
      free(prev_aggregate_mac);

      if (ret != SGX_SUCCESS) return SGX_ERROR_UNEXPECTED;
    } else {
      // no need to verify
    }
  } else {
    prev_aggregate_mac = (uint8_t *)malloc(MAC_SIZE * total_mac_count);
    memset(prev_aggregate_mac, 0, MAC_SIZE * total_mac_count);

    memcpy(prev_aggregate_mac, aggregate_mac, MAC_SIZE * total_mac_count);
    memcpy(prev_aggregate_mac + updated_idx * MAC_SIZE, prev_mac, MAC_SIZE);

    // verify tree root using previous mac
    ret = sgx_rijndael128_cmac_msg(&gsk, prev_aggregate_mac,
                                   MAC_SIZE * total_mac_count, &temp_mac);
    free(prev_aggregate_mac);

    if (ret != SGX_SUCCESS) return SGX_ERROR_UNEXPECTED;
  }

  // If ShieldStore stores at least one entry, we can verify tree root
  if (!is_insert || (is_insert && total_mac_count > 1)) {
    if (memcmp(temp_mac, MACTable[hash_val / ratio_root_per_buckets].mac,
               MAC_SIZE) != 0) {
      print_error("Previous MAC compare failed");
      return SGX_ERROR_UNEXPECTED;
    }
  }

  // updated tree root
  ret = sgx_rijndael128_cmac_msg(&gsk, aggregate_mac,
                                 MAC_SIZE * total_mac_count, &temp_mac);
  free(aggregate_mac);
  memcpy(MACTable[hash_val / ratio_root_per_buckets].mac, temp_mac, MAC_SIZE);

  return ret;
}

/**
 * verify integrity tree
 **/
sgx_status_t enclave_verify_tree_root(int hash_val) {
  uint8_t cur_mac[MAC_SIZE];

  get_chain_mac(hash_val, cur_mac);

  if (memcmp(cur_mac, MACTable[hash_val / ratio_root_per_buckets].mac,
             MAC_SIZE) != 0)
    return SGX_ERROR_UNEXPECTED;

  return SGX_SUCCESS;
}

/**
 * encrypt and generate MAC value of entry
 **/
void enclave_encrypt(char *key_val, char *cipher, uint8_t key_idx,
                     uint32_t key_len, uint32_t val_len, uint8_t *nac,
                     uint8_t *mac) {
  uint8_t *temp_cipher;
  uint8_t *temp_all;
  uint8_t temp_nac[NAC_SIZE];
  uint8_t temp_mac[MAC_SIZE];

  memcpy(temp_nac, nac, NAC_SIZE);

  temp_cipher = (uint8_t *)cipher;
  temp_all =
      (uint8_t *)malloc(sizeof(uint8_t) * (key_len + val_len + NAC_SIZE + 1) +
                        sizeof(uint32_t) * 2);

  /* Encrypt plain text first */
  sgx_status_t ret =
      sgx_aes_ctr_encrypt(&gsk, (uint8_t *)key_val, key_len + val_len, temp_nac,
                          NAC_SIZE, temp_cipher);

  if (ret != SGX_SUCCESS) {
    assert(0);
  }

  /* Generate MAC */
  memcpy(temp_all, temp_cipher, key_len + val_len);
  memcpy(temp_all + key_len + val_len, nac, NAC_SIZE);
  memcpy(temp_all + key_len + val_len + NAC_SIZE, &key_idx, sizeof(uint8_t));
  memcpy(temp_all + key_len + val_len + NAC_SIZE + sizeof(uint8_t), &key_len,
         sizeof(uint32_t));
  memcpy(temp_all + key_len + val_len + NAC_SIZE + sizeof(uint8_t) +
             sizeof(uint32_t),
         &val_len, sizeof(uint32_t));
  sgx_rijndael128_cmac_msg(
      &gsk, temp_all,
      key_len + val_len + NAC_SIZE + sizeof(uint8_t) + sizeof(uint32_t) * 2,
      &temp_mac);

  /* Copy results to outside of enclave */
  cipher = (char *)temp_cipher;
  memcpy(mac, temp_mac, MAC_SIZE);

  free(temp_all);
}

/**
 * verify integrity with MAC value
 **/
sgx_status_t enclave_verification(char *cipher, uint8_t key_idx,
                                  uint32_t key_len, uint32_t val_len,
                                  uint8_t *nac, uint8_t *mac) {
  uint8_t *temp_all;
  uint8_t temp_nac[NAC_SIZE];
  uint8_t temp_mac[MAC_SIZE];

  /* Generate nac */
  memcpy(temp_nac, nac, NAC_SIZE);

  temp_all =
      (uint8_t *)malloc(sizeof(uint8_t) * (key_len + val_len + NAC_SIZE + 1) +
                        sizeof(uint32_t) * 2);

  memcpy(temp_all, cipher, key_len + val_len);
  memcpy(temp_all + key_len + val_len, temp_nac, NAC_SIZE);
  memcpy(temp_all + key_len + val_len + NAC_SIZE, &key_idx, sizeof(uint8_t));
  memcpy(temp_all + key_len + val_len + NAC_SIZE + sizeof(uint8_t), &key_len,
         sizeof(uint32_t));
  memcpy(temp_all + key_len + val_len + NAC_SIZE + sizeof(uint8_t) +
             sizeof(uint32_t),
         &val_len, sizeof(uint32_t));
  sgx_status_t ret = sgx_rijndael128_cmac_msg(
      &gsk, temp_all,
      key_len + val_len + NAC_SIZE + sizeof(uint8_t) + sizeof(uint32_t) * 2,
      &temp_mac);

  /* When MAC is same */
  if (memcmp(temp_mac, mac, MAC_SIZE) == 0) {
    ;
  }
  /* When MAC is not same */
  else {
    print_error("MAC matching failed");
  }

  free(temp_all);

  return ret;
}

/**
 * decrypt entry and check MAC value and entry
 **/
void enclave_decrypt(char *cipher, char *plain, uint8_t key_idx,
                     uint32_t key_len, uint32_t val_len, uint8_t *nac,
                     uint8_t *mac) {
  uint8_t *temp_plain;
  uint8_t *temp_all;
  uint8_t temp_nac[NAC_SIZE];
  uint8_t temp_mac[MAC_SIZE];

  /* Generate nac */
  memcpy(temp_nac, nac, NAC_SIZE);

  /* Generate MAC & compare it */
  temp_plain = (uint8_t *)plain;
  temp_all =
      (uint8_t *)malloc(sizeof(uint8_t) * (key_len + val_len + NAC_SIZE + 1) +
                        sizeof(uint32_t) * 2);

  memcpy(temp_all, cipher, key_len + val_len);
  memcpy(temp_all + key_len + val_len, temp_nac, NAC_SIZE);
  memcpy(temp_all + key_len + val_len + NAC_SIZE, &key_idx, sizeof(uint8_t));
  memcpy(temp_all + key_len + val_len + NAC_SIZE + sizeof(uint8_t), &key_len,
         sizeof(uint32_t));
  memcpy(temp_all + key_len + val_len + NAC_SIZE + sizeof(uint8_t) +
             sizeof(uint32_t),
         &val_len, sizeof(uint32_t));
  sgx_status_t ret = sgx_rijndael128_cmac_msg(
      &gsk, temp_all,
      key_len + val_len + NAC_SIZE + sizeof(uint8_t) + sizeof(uint32_t) * 2,
      &temp_mac);

  if (ret != SGX_SUCCESS) {
    assert(0);
  }

  /* When MAC is same */
  if (memcmp(temp_mac, mac, MAC_SIZE) == 0) {
    /* Decrypt cipher text */
    sgx_aes_ctr_decrypt(&gsk, (uint8_t *)cipher, key_len + val_len, temp_nac,
                        NAC_SIZE, temp_plain);
    /* Copy results to outside of enclave */
    plain = (char *)temp_plain;
  }
  /* When MAC is not same */
  else {
    print_error("MAC matching failed");
  }

  free(temp_all);
}

/**
 * init enclave values
 **/
void enclave_init_values(hashtable *ht_, MACbuffer *MACbuf_, Arg arg) {
  ht_enclave = ht_;
  MACbuf_enclave = MACbuf_;

  arg_enclave = arg;

  // set the ratio of subtree root node inside the enclave memory
  // over total hash value of buckets
  ratio_root_per_buckets = ht_enclave->size / arg_enclave.tree_root_size;

  MACTable =
      (BucketMAC *)malloc(sizeof(BucketMAC) * arg_enclave.tree_root_size);
  for (int i = 0; i < arg_enclave.tree_root_size; i++) {
    // memset(MACTable[i].mac, 0, MAC_SIZE);
    BucketMAC bucket = MACTable[i];
    memset(bucket.mac, 0, MAC_SIZE);
  }

  // Initialize mutex variables
  sgx_thread_mutex_init(&global_mutex, NULL);

  queue_mutex = (sgx_thread_mutex_t *)malloc(sizeof(sgx_thread_mutex_t) *
                                             arg_enclave.num_threads);
  job_cond = (sgx_thread_cond_t *)malloc(sizeof(sgx_thread_cond_t) *
                                         arg_enclave.num_threads);
  for (int i = 0; i < arg_enclave.num_threads; i++) {
    queue.push_back(std::queue<job *>());
  }
}

/**
 * Bring the resquest to enclave
 * parsing the key and send the requests to specific thread
 **/
void enclave_message_pass(void *data) {
  Command command = ((job *)data)->command;

  char *key;
  uint32_t key_size;
  char *tok;
  char *temp_;
  int thread_id = 0;

  job *new_job = NULL;

  switch (command) {
    case QUIT:
      print_info("Received QUIT");

      // Send exit message to all of the worker threads
      for (int i = 0; i < arg_enclave.num_threads; i++) {
        print_info("Sending QUIT to all threads");
        new_job = (job *)malloc(sizeof(job));
        new_job->command = QUIT;
        new_job->signature = ((job *)data)->signature;

        sgx_thread_mutex_lock(&queue_mutex[i]);
        queue[i].push(new_job);
        sgx_thread_cond_signal(&job_cond[i]);
        sgx_thread_mutex_unlock(&queue_mutex[i]);
      }
      break;

    case SHARED:
    case EXCLUSIVE:
    case UNLOCK:
      print_info("Received lock request");

      new_job = (job *)malloc(sizeof(job));
      new_job->command = ((job *)data)->command;
      new_job->transaction_id = ((job *)data)->transaction_id;
      new_job->row_id = ((job *)data)->row_id;
      new_job->signature = ((job *)data)->signature;

      // Send the requests to specific worker thread
      thread_id = (int)(ht_hash(new_job->row_id) /
                        (ht_enclave->size / arg_enclave.num_threads));
      sgx_thread_mutex_lock(&queue_mutex[thread_id]);
      queue[thread_id].push(new_job);
      sgx_thread_cond_signal(&job_cond[thread_id]);
      sgx_thread_mutex_unlock(&queue_mutex[thread_id]);
      break;

    default:
      print_error("Received unknown command");
      break;
  }
  /*
    new_job = (job *)malloc(sizeof(job));
    new_job->buf = (char *)malloc(sizeof(char) * arg_enclave.max_buf_size);
    memcpy(new_job->buf, cipher, arg_enclave.max_buf_size);
    new_job->client_sock = client_sock;

    // parsing key
    tok = strtok_r(cipher + 4, " ", &temp_);
    key_size = strlen(tok) + 1;
    key = (char *)malloc(sizeof(char) * key_size);
    memset(key, 0, key_size);
    memcpy(key, tok, key_size - 1);

    // send the requests to specific worker thread
    thread_id =
        (int)(ht_hash(key) / (ht_enclave->size / arg_enclave.num_threads));
    free(key);
    sgx_thread_mutex_lock(&queue_mutex[thread_id]);
    queue[thread_id].push(new_job);
    sgx_thread_cond_signal(&job_cond[thread_id]);
    sgx_thread_mutex_unlock(&queue_mutex[thread_id]);
  }*/

  return;
}

/**
 * processing set operation
 **/
void enclave_set(char *cipher) {
  char *key;
  char *val;
  char *key_val;
  char *plain_key_val = NULL;
  uint8_t nac[NAC_SIZE];
  uint8_t mac[MAC_SIZE];
  uint8_t prev_mac[MAC_SIZE];
  uint8_t updated_nac[NAC_SIZE];

  uint32_t key_size;
  uint32_t val_size;

  uint8_t key_idx;

  char *tok;
  char *temp_;
  entry *ret_entry;

  bool is_insert = false;

  tok = strtok_r(cipher + 4, " ", &temp_);
  key_size = strlen(tok) + 1;
  key = (char *)malloc(sizeof(char) * key_size);
  memset(key, 0, key_size);
  memcpy(key, tok, key_size - 1);

  val_size = strlen(temp_) + 1;
  val = (char *)malloc(sizeof(char) * val_size);
  memset(val, 0, val_size);
  memcpy(val, temp_, val_size - 1);

  int kv_pos = -1;

  ret_entry = ht_get_o(key, key_size, &plain_key_val, &kv_pos, updated_nac);

  int hash_val = ht_hash(key);
  key_idx = key_hash_func(key);

  sgx_status_t ret = SGX_SUCCESS;

  /* verifying integrity of data */
  // update
  if (ret_entry != NULL) {
    ret = enclave_verification(ret_entry->key_val, ret_entry->key_hash,
                               ret_entry->key_size, ret_entry->val_size,
                               ret_entry->nac, ret_entry->mac);

    if (ret != SGX_SUCCESS) {
      print_error("MAC verification failed");
    }

    memcpy(nac, updated_nac, NAC_SIZE);
    free(plain_key_val);

    is_insert = false;
    memcpy(prev_mac, ret_entry->mac, MAC_SIZE);
  }
  // insert
  else {
    /* Make initial nac */
    sgx_read_rand(nac, NAC_SIZE);
    assert(plain_key_val == NULL);
    is_insert = true;
  }

  /* We have to encrypt key and value together, so make con field */
  key_val = (char *)malloc(sizeof(char) * (key_size + val_size));
  memcpy(key_val, key, key_size);
  memcpy(key_val + key_size, val, val_size);

  enclave_encrypt(key_val, key_val, key_idx, key_size, val_size, nac, mac);

  ht_set_o(ret_entry, key, key_val, nac, mac, key_size, val_size, kv_pos);

  ret = enclave_rebuild_tree_root(hash_val, kv_pos, is_insert, prev_mac);

  if (ret != SGX_SUCCESS) {
    print_error("Tree verification failed");
  }

  memset(cipher, 0, arg_enclave.max_buf_size);
  memcpy(cipher, key, key_size);

  free(key);
  free(val);
  free(key_val);
}

/**
 * processing get operation
 **/
void enclave_get(char *cipher) {
  char *key;
  char *plain_key_val = NULL;
  uint32_t key_size;

  char *tok;
  char *temp_;
  entry *ret_entry;

  uint8_t updated_nac[NAC_SIZE];

  tok = strtok_r(cipher + 4, " ", &temp_);
  key_size = strlen(tok) + 1;
  key = (char *)malloc(sizeof(char) * key_size);
  memset(key, 0, key_size);
  memcpy(key, tok, key_size - 1);

  int kv_pos = -1;
  ret_entry = ht_get_o(key, key_size, &plain_key_val, &kv_pos, updated_nac);

  if (ret_entry == NULL) {
    print_warn("GET FAILED: No data in database");
    return;
  }

  int hash_val = ht_hash(key);
  sgx_status_t ret = SGX_SUCCESS;

  /* verifying integrity of data */
  ret = enclave_verification(ret_entry->key_val, ret_entry->key_hash,
                             ret_entry->key_size, ret_entry->val_size,
                             ret_entry->nac, ret_entry->mac);

  if (ret != SGX_SUCCESS) {
    print_error("MAC verification failed");
  }

  ret = enclave_verify_tree_root(hash_val);

  if (ret != SGX_SUCCESS) {
    print_error("Tree verification failed");
  }

  memset(cipher, 0, arg_enclave.max_buf_size);
  memcpy(cipher, plain_key_val + ret_entry->key_size, ret_entry->val_size);

  free(key);
  free(plain_key_val);
}

/**
 * processing append operation
 **/
void enclave_append(char *cipher) {
  char *key;
  char *val;
  char *key_val;
  char *plain_key_val;
  uint8_t nac[NAC_SIZE];
  uint8_t mac[MAC_SIZE];
  uint8_t prev_mac[MAC_SIZE];
  uint8_t updated_nac[NAC_SIZE];

  uint32_t key_size;
  uint32_t val_size;

  uint8_t key_idx;

  char *tok;
  char *temp_;
  entry *ret_entry;

  tok = strtok_r(cipher + 4, " ", &temp_);
  key_size = strlen(tok) + 1;
  key = (char *)malloc(sizeof(char) * key_size);
  memset(key, 0, key_size);
  memcpy(key, tok, key_size - 1);

  val_size = strlen(temp_) + 1;
  val = (char *)malloc(sizeof(char) * val_size);
  memset(val, 0, val_size);
  memcpy(val, temp_, val_size - 1);

  int kv_pos = -1;

  ret_entry = ht_get_o(key, key_size, &plain_key_val, &kv_pos, updated_nac);

  int hash_val = ht_hash(key);
  key_idx = key_hash_func(key);

  sgx_status_t ret = SGX_SUCCESS;

  /* verifying integrity of data */
  // update
  if (ret_entry != NULL) {
    ret = enclave_verification(ret_entry->key_val, ret_entry->key_hash,
                               ret_entry->key_size, ret_entry->val_size,
                               ret_entry->nac, ret_entry->mac);

    if (ret != SGX_SUCCESS) {
      print_error("MAC verification failed");
    }

    memcpy(nac, updated_nac, NAC_SIZE);
    memcpy(prev_mac, ret_entry->mac, MAC_SIZE);
  }
  // insert
  else {
    print_warn("There's no data in the database");
    return;
  }

  /* Make appended key-value */
  key_val = (char *)malloc(sizeof(char) * (ret_entry->key_size +
                                           ret_entry->val_size + val_size - 1));
  memcpy(key_val, plain_key_val, ret_entry->key_size + ret_entry->val_size);
  memcpy(key_val + ret_entry->key_size + ret_entry->val_size - 1, val,
         val_size);

  enclave_encrypt(key_val, key_val, key_idx, key_size,
                  ret_entry->val_size + val_size - 1, nac, mac);

  ht_append_o(ret_entry, key, key_val, nac, mac, key_size,
              ret_entry->val_size + val_size - 1, kv_pos);

  ret = enclave_rebuild_tree_root(hash_val, kv_pos, false, prev_mac);

  if (ret != SGX_SUCCESS) {
    print_error("Tree verification failed");
  }

  memset(cipher, 0, arg_enclave.max_buf_size);
  memcpy(cipher, key, key_size);

  free(key);
  free(val);
  free(plain_key_val);
  free(key_val);
}

/**
 * processing server working threads
 **/
void enclave_worker_thread(hashtable *ht_, MACbuffer *MACbuf_) {
  int thread_id;
  job *cur_job = NULL;

  ht_enclave = ht_;
  MACbuf_enclave = MACbuf_;

  sgx_thread_mutex_lock(&global_mutex);

  thread_id = num;
  num += 1;

  sgx_thread_mutex_init(&queue_mutex[thread_id], NULL);
  sgx_thread_cond_init(&job_cond[thread_id], NULL);

  sgx_thread_mutex_unlock(&global_mutex);

  sgx_thread_mutex_lock(&queue_mutex[thread_id]);

  while (1) {
    print_info("Enclave worker waiting for jobs");
    if (queue[thread_id].size() == 0) {
      sgx_thread_cond_wait(&job_cond[thread_id], &queue_mutex[thread_id]);
      continue;
    }

    print_info("Worker got a job");
    cur_job = queue[thread_id].front();
    Command command = cur_job->command;

    sgx_thread_mutex_unlock(&queue_mutex[thread_id]);

    switch (command) {
      case QUIT:
        sgx_thread_mutex_lock(&queue_mutex[thread_id]);
        queue[thread_id].pop();
        cur_job->signature[0] = 'x';
        free(cur_job);
        sgx_thread_mutex_unlock(&queue_mutex[thread_id]);

        sgx_thread_mutex_destroy(&queue_mutex[thread_id]);
        sgx_thread_cond_destroy(&job_cond[thread_id]);
        print_info("Enclave worker quitting");
        return;
      case SHARED:
        print_info("Worker received SHARED");
        break;
      case EXCLUSIVE:
        print_info("Worker received EXCLUSIVE");
        break;
      case UNLOCK:
        print_info("Worker received UNLOCK");
        break;
      default:
        print_error("Worker received unknown command");
    }

    /*if (strncmp(cipher, "GET", 3) == 0 || strncmp(cipher, "get", 3) == 0) {
      enclave_get(cipher);
    } else if (strncmp(cipher, "SET", 3) == 0 ||
               strncmp(cipher, "set", 3) == 0) {
      enclave_set(cipher);
    } else if (strncmp(cipher, "APP", 3) == 0 ||
               strncmp(cipher, "app", 3) == 0) {
      enclave_append(cipher);
    } else if (strncmp(cipher, "quit", 4) == 0) {
      sgx_thread_mutex_lock(&queue_mutex[thread_id]);
      queue[thread_id].pop();
      cur_job->signature[0] = 'x';
      free(cipher);
      free(cur_job);
      sgx_thread_mutex_unlock(&queue_mutex[thread_id]);

      sgx_thread_mutex_destroy(&queue_mutex[thread_id]);
      sgx_thread_cond_destroy(&job_cond[thread_id]);
      print_info("enclave worker quitting");
      return;
    } else {
      print_warn("Untyped request");
      break;
    }*/

    sgx_thread_mutex_lock(&queue_mutex[thread_id]);
    queue[thread_id].pop();
    free(cur_job);
  }

  return;
}

auto get_block_timeout() -> unsigned int {
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
  sgx_status_t ret =
      sgx_ecdsa_verify((uint8_t *)message, strnlen(message, MAX_MESSAGE_LENGTH),
                       &ec256_public_key, sig, &res, context);
  return res;
}

// Closes the ECDSA context
auto ecdsa_close() -> int {
  if (context == NULL) sgx_ecc256_open_context(&context);
  return sgx_ecc256_close_context(context);
}

int register_transaction(unsigned int transactionId, unsigned int lockBudget) {
  print_info("Registering transaction");
  if (transactionTable_.contains(transactionId)) {
    print_error("Transaction is already registered");
    return SGX_ERROR_UNEXPECTED;
  }

  transactionTable_.set(
      transactionId, std::make_shared<Transaction>(transactionId, lockBudget));

  return SGX_SUCCESS;
}

int acquire_lock(void *signature, size_t sig_len, unsigned int transactionId,
                 unsigned int rowId, int isExclusive) {
  // Get the transaction object for the given transaction ID
  std::shared_ptr<Transaction> transaction =
      transactionTable_.get(transactionId);
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return SGX_ERROR_UNEXPECTED;
  }
  // Get the lock object for the given row ID
  std::shared_ptr<Lock> lock = lockTable_.get(rowId);
  if (lock == nullptr) {
    lock = std::make_shared<Lock>();
    lockTable_.set(rowId, lock);
  }

  // Check if 2PL is violated
  if (transaction->getPhase() == Transaction::Phase::kShrinking) {
    print_error("Cannot acquire more locks according to 2PL");
    goto abort;
  }

  // Check if lock budget is enough
  if (transaction->getLockBudget() < 1) {
    print_error("Lock budget exhausted");
    goto abort;
  }

  // Check for upgrade request
  if (transaction->hasLock(rowId) && isExclusive &&
      lock->getMode() == Lock::LockMode::kShared) {
    lock->upgrade(transactionId);
    goto sign;
  }

  // Acquire lock in requested mode (shared, exclusive)
  if (!transaction->hasLock(rowId)) {
    if (isExclusive) {
      transaction->addLock(rowId, Lock::LockMode::kExclusive, lock);
    } else {
      transaction->addLock(rowId, Lock::LockMode::kShared, lock);
    }
    goto sign;
  }

  print_error("Request for already acquired lock");

abort:
  abort_transaction(transaction);
  return SGX_ERROR_UNEXPECTED;

sign:
  unsigned int block_timeout = get_block_timeout();
  /* Get string representation of the lock tuple:
   * <TRANSACTION-ID>_<ROW-ID>_<MODE>_<BLOCKTIMEOUT>,
   * where mode means, if the lock is for shared or exclusive access
   */
  std::string mode;
  switch (lockTable_.get(rowId)->getMode()) {
    case Lock::LockMode::kExclusive:
      mode = "X";  // exclusive
      break;
    case Lock::LockMode::kShared:
      mode = "S";  // shared
      break;
  };

  std::string string_to_sign = std::to_string(transactionId) + "_" +
                               std::to_string(rowId) + "_" + mode + "_" +
                               std::to_string(block_timeout);

  sgx_ecc_state_handle_t context = NULL;
  sgx_ecc256_open_context(&context);
  sgx_ecdsa_sign((uint8_t *)string_to_sign.c_str(),
                 strnlen(string_to_sign.c_str(), MAX_MESSAGE_LENGTH),
                 &ec256_private_key, (sgx_ec256_signature_t *)signature,
                 context);

  int ret = verify(string_to_sign.c_str(), (void *)signature,
                   sizeof(sgx_ec256_signature_t));
  if (ret != SGX_SUCCESS) {
    print_error("Failed to verify signature");
  } else {
    print_info("Signature successfully verified");
  }

  return ret;
}

void release_lock(unsigned int transactionId, unsigned int rowId) {
  // Get the transaction object
  std::shared_ptr<Transaction> transaction;
  transaction = transactionTable_.get(transactionId);
  if (transaction == nullptr) {
    print_error("Transaction was not registered");
    return;
  }

  // Get the lock object
  std::shared_ptr<Lock> lock;
  lock = lockTable_.get(rowId);
  if (lock == nullptr) {
    print_error("Lock does not exist");
    return;
  }

  transaction->releaseLock(rowId, lock);
}

void abort_transaction(const std::shared_ptr<Transaction> &transaction) {
  transactionTable_.erase(transaction->getTransactionId());
  transaction->releaseAllLocks(lockTable_);
}
