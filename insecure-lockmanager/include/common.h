#pragma once

#include <stdbool.h>

enum Command { SHARED, EXCLUSIVE, UNLOCK, QUIT, REGISTER };

struct Job {
  enum Command command;
  unsigned int transaction_id;
  unsigned int row_id;
  volatile bool* finished;
  volatile bool* error;
};
typedef struct Job Job;  // Required to use C++ structs as C structs

struct Arg {
  int num_threads;
  int tx_thread_id;
  int transaction_table_size;
  int lock_table_size;
};
typedef struct Arg Arg;  // Required to use C++ structs as C structs