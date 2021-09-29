#pragma once

#include <stdlib.h>

#include "common.h"
#include "lock.h"
#include "transaction.h"

HashTable* newHashTable(int size);

Entry* newEntry(int key, void* value);

/**
 * Maps each key to an index from 0..size-1 within the hashtable
 *
 * @param hashTable either the lock or transaction table to execute the
 * operation on
 * @param key TXID or RID to map into the hashtable
 */
int hash(HashTable* hashTable, int key);

/**
 * Retrieves the value for the given key.
 *
 * @param hashTable either the lock or transaction table to execute the
 * operation on
 * @param key idedentifies the value, i.e. TXID or RID
 * @returns value for the given key. Needs to be casted to the appropriate
 * pointer type, i.e. Transaction or Lock. Or nullptr, when the corresponding
 * key could not be found.
 */
auto get(HashTable* hashTable, int key) -> void*;

/**
 *  Sets the value for a given key. Doesn't do anything when the key already
 * exists in the table.
 *
 * @param hashTable either the lock or transaction table to execute the
 * operation on
 * @param key TXID or RID
 * @param value needs to be casted to void pointer from Transaction or Lock
 * pointer
 */
void set(HashTable* hashTable, int key, void* value);

/**
 * Checks if the transaction got already registered or a lock already exists
 * for the given key
 *
 * @param hashTable either the lock or transaction table to execute the
 * operation on
 * @param key TXID or RID
 * @return true if the transaction or the lock respectively is in the
 * hashTable
 */
auto contains(HashTable* hashTable, int key) -> bool;

/**
 * Deletes the lock or transaction respectively for the given key
 *
 * @param hashTable either the lock or transaction table to execute the
 * operation on
 * @param key TXID or RID
 */
void remove(HashTable* hashTable, int key);

/**
 * This function takes an entry of a lock table and serializes it into uint8_t*,
 * which is the required parameter type for the sgx_sha256_hash function.
 *
 * @param entry the entry that should be serialized to a uint8_t* (the
 * entry should contain locks as values)
 * @param result the serialized entry
 * @returns the size of the serialized entry
 */
auto locktable_entry_to_uint8_t(Entry* entry, uint8_t* result) -> uint32_t;

/**
 * This function takes an entry of a lock table and serializes it into uint8_t*,
 * which is the required parameter type for the sgx_sha256_hash function.
 *
 * @param entry the entry that should be serialized to a uint8_t* (the
 * entry should contain transactions as values)
 * @param result the serialized entry
 * @returns the size of the serialized entry
 */
auto transactiontable_entry_to_uint8_t(Entry* entry, uint8_t* result)
    -> uint32_t;