#pragma once

#include <stdbool.h>

/**
 * This struct is used either as a transaction table, where the keys
 * resemble the TXIDs and the value the transaction structs or a lock table,
 * with RIDs as keys and lock structs as values.
 */
typedef struct {
  int size;
  struct Entry** table;
} HashTable;
// typedef HashTable HashTable;

typedef struct Entry Entry;
struct Entry {
  int key;
  void* value;
  struct Entry* next;
};

enum Command { SHARED, EXCLUSIVE, UNLOCK, QUIT, REGISTER };

struct Job {
  enum Command command;
  unsigned int transaction_id;
  unsigned int row_id;
  unsigned int lock_budget;
  volatile char* return_value;
  volatile bool* finished;
  volatile bool* error;
};
typedef struct Job Job;

struct Arg {
  int num_threads;
  int tx_thread_id;
  int transaction_table_size;
  int lock_table_size;
};
typedef struct Arg Arg;