#include "hashtable.h"

HashTable* newHashTable(int size) {
  HashTable* hashTable = new HashTable();
  hashTable->size = size;
  hashTable->table = new Entry*[size];
  for (int i = 0; i < size; i++) {
    hashTable->table[i] = nullptr;
  }
  hashTable->bucketSizes = new unsigned int[size];
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
  return get(entry, key);
}

auto get(Entry* entry, int key) -> void* {
  while (entry != nullptr) {
    if (entry->key == key) {
      return entry->value;
    }
    entry = entry->next;
  }
  return nullptr;
}

void set(HashTable* hashTable, int key, void* value) {
  int position = hash(hashTable->size, key);
  Entry* entry = hashTable->table[position];

  Entry* entryToInsert = new Entry();
  entryToInsert->key = key;
  entryToInsert->value = value;
  entryToInsert->next = nullptr;

  if (entry == nullptr) {
    hashTable->table[position] = entryToInsert;
    hashTable->bucketSizes[position]++;
    return;
  }

  while (entry->next != nullptr) {
    if (entry->key == key) {
      delete entryToInsert;
      return;  // key already exists
    }
    entry = entry->next;
  }

  entry->next = entryToInsert;  // Add new entry at the end of the list
  hashTable->bucketSizes[position]++;
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
  int position = hash(hashTable->size, key);
  Entry* entry = hashTable->table[position];

  if (entry == nullptr) {
    return;
  }

  if (entry->key == key) {
    hashTable->table[position] = entry->next;
    hashTable->bucketSizes[position]--;
    // delete entry; We don't want to free the memory here, as deleting
    // untrusted memory from within the enclave causes an error
    return;
  }

  Entry* next = entry->next;
  while (next != nullptr) {
    if (next->key == key) {
      // delete it and return
      entry->next = next->next;
      hashTable->bucketSizes[position]--;
      // delete next; We don't want to free the memory here, as deleting
      // untrusted memory from within the enclave causes causes an error
      return;
    }
    entry = next;
    next = entry->next;
  }
}