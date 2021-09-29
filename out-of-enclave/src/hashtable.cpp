#include "hashtable.h"

HashTable* newHashTable(int size) {
  HashTable* hashTable = new HashTable();
  hashTable->size = size;
  hashTable->table = new Entry*[size];
  for (int i = 0; i < size; i++) {
    hashTable->table[i] = nullptr;
  }
  return hashTable;
};

Entry* newEntry(int key, void* value) {
  Entry* entry = new Entry();
  entry->key = key;
  entry->value = value;
  return entry;
}

int hash(int size, int key) { return key % size; }

auto get(HashTable* hashTable, int key) -> void* {
  Entry* entry = hashTable->table[hash(hashTable->size, key)];

  while (entry != nullptr) {
    if (entry->key == key) {
      return entry->value;
    }
    entry = entry->next;
  }

  return nullptr;
}

void set(HashTable* hashTable, int key, void* value) {
  Entry* entry = hashTable->table[hash(hashTable->size, key)];

  Entry* entryToInsert = new Entry();
  entryToInsert->key = key;
  entryToInsert->value = value;
  entryToInsert->next = nullptr;

  if (entry == nullptr) {
    hashTable->table[hash(hashTable->size, key)] = entryToInsert;
    return;
  }

  while (entry->next != nullptr) {
    if (entry->key == key) {
      delete entryToInsert;
      return;  // key already exists
    }
    entry = entry->next;
  }

  entry->next = entryToInsert;
}

auto contains(HashTable* hashTable, int key) -> bool {
  Entry* entry = hashTable->table[hash(hashTable->size, key)];

  while (entry != nullptr) {
    if (entry->key == key) {
      return true;
    }
    entry = entry->next;
  }

  return false;
}

void remove(HashTable* hashTable, int key) {
  Entry* entry = hashTable->table[hash(hashTable->size, key)];

  if (entry == nullptr) {
    return;
  }

  if (entry->key == key) {
    // hashTable->table[hash(hashTable->size, key)] = nullptr;
    return;
  }

  Entry* next = entry->next;
  while (next != nullptr) {
    if (next->key == key) {
      // delete it and return
      entry->next = next->next;
      return;
    }
    entry = next;
    next = entry->next;
  }
}

auto locktable_entry_to_uint8_t(Entry* entry, uint8_t* result) -> uint32_t {
  Lock* lock = (Lock*)(entry->value);
  int num_owners = lock->num_owners;
  uint32_t size = 3 + num_owners;

  // Get the entry key and every member of the lock struct
  result = new uint8_t[size];
  result[0] = entry->key;
  result[1] = lock->exclusive;
  result[2] = num_owners;

  for (int i = 0; i < num_owners; i++) {
    result[3 + i] = lock->owners[i];
  }

  return size;
}

auto transactiontable_entry_to_uint8_t(Entry* entry, uint8_t* result)
    -> uint32_t {
  Transaction* transaction = (Transaction*)(entry->value);
  int num_locked = transaction->num_locked;
  uint32_t size = 6 + num_locked;

  // Get the entry key and every member of the transaction struct
  result = new uint8_t[size];
  result[0] = entry->key;
  result[1] = transaction->transaction_id;
  result[2] = transaction->aborted;
  result[3] = transaction->growing_phase;
  result[4] = transaction->lock_budget;
  result[5] = transaction->locked_rows_size;
  result[6] = num_locked;

  for (int i = 0; i < num_locked; i++) {
    result[7 + i] = transaction->locked_rows[i];
  }

  return size;
}