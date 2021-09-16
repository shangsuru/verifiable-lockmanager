#pragma once

#include <stdbool.h>

enum Command { SHARED, EXCLUSIVE, UNLOCK, QUIT, REGISTER };

struct Job {
  enum Command command;
  unsigned int transaction_id;
  unsigned int row_id;
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