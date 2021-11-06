#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "lockmanager.h"

using std::ofstream;
using std::reduce;
using std::string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;

const size_t bigger_than_cachesize =
    20 * 1024 * 1024;  // Checked cache size with command 'lscpu | grep cache'
long* p = new long[bigger_than_cachesize];
int transactionA = 1;
int transactionB = 2;

/*
vector<int> lockBudgets = {
    10,    100,   500,   1000,   2500,   5000,
    10000, 20000, 50000, 100000, 150000, 200000};  // how many locks to acquire
    */
vector<int> lockBudgets = {10};
const int lockTableSize = 10000;
const int repetitions = 10;  // repeats the same experiments several times
int numWorkerThreads =
    1;  // this is just written to the CSV file and has no influence
        // on the actual number of worker threads. These need to be
        // adapted separately in the respective lockmanager.cpp file
        // (search for "arg.num_threads")

void flushCache() {
  for (int i = 0; i < bigger_than_cachesize; i++) {
    p[i] = rand();
  }
}

/**
 * Writes the data all in one into a CSV file
 * @param filename the name of the csv file
 * @param values the outer vector contains the rows and the inner vector
 * resembles a row with its column values
 */
void writeToCSV(string filename, vector<vector<long>> values) {
  ofstream file;
  file.open(filename + ".csv");

  for (const auto& row : values) {
    for (int i = 0; i < row.size() - 1; i++) {
      file << row[i] << ",";
    }
    file << row[row.size() - 1];
    file << std::endl;
  }
  file.close();
}

/**
 * Highlevel description of the experiment:
 * A acquires given number of locks in shared mode, then B acquires the same
 * locks in shared mode. This makes A to write lock objects into the lock table
 * and B read them again later on. This is an attempt to show the overhead of
 * paging in Intel SGX enclaves.
 *
 * With multiple concurrent requests (we achieve this by having the client not
 * wait on the result of its requests), we have to make sure that we do wait on
 * the last request for each thread, e.g. when there are two threads we need to
 * make the last request that each of these threads serves wait for its result,
 * so that we wait on all requests to be finished.
 */
void experiment(LockManager& lockManager, int numLocks, int numThreads) {
  /** Compute RIDs to wait on depending on number of locks and number of
   threads. The size of the lock table is 10000. Remember, each RID is assigned
   to a thread ID via
   *
   * ===========================================================================
   * int thread_id = (int)((new_job.row_id % lockTableSize_) /
                            ((float)lockTableSize_ / (arg.num_threads - 1)));
   * ===========================================================================
   *
   * which is explained in more detail below. We want to find the latest request
   * (i.e. request with the largest RID) for
   * each thread, which needs to be synchronous, so that each thread completes
   * all requests before we stop the time measurement.
   *
   * As you see from the formula above, a
   * set of RIDs less than the lockTableSize_ = 10000 doesn't distribute over
   * all threads. I.e., when the range is only between 1-5000 and we have 4
   * threads, the fourth thread, who has the range of 7500 - 9999, never
   * receives a request. This can be a disadvantage when RIDs are skewed.
   */
  std::set<int> waitOn;

  // The maximum RID is in any case one of the last requests.
  waitOn.insert(numLocks);

  /**
   * As you see in the second line of the formula, the lock table ranging from 0
   * to 9999 (10000 entries) is divided evenly among the worker threads, which
   * is the number of threads in the enclave - 1 (which is the single thread
   * responsible for the transaction table). As for the partitions, e.g. for 4
   * threads, thread 0 takes RIDs 0 - 2499, thread 1 takes RID 2500 - 4999
   * a.s.o.
   *
   * If the RID is 10000 and bigger, the RID is projected into the range of the
   * lock table from 0 to 9999 using modulo (see line 1 of the formula). When we
   * want to find the largest ID to wait on, we first compute the "base", which
   * for numLocks 1 - 60000 would be 50000. That means the largest RID for each
   * thread will be in the range 50000 - 60000. The RIDs to wait on for 4
   * threads would be 60000, 54999, a.s.o, compare with the ranges
   * computed in the above paragraph and it should get clear.
   */
  int base = ((numLocks / lockTableSize) - 1) * lockTableSize;
  if (numLocks < lockTableSize) {
    base = 0;
  }

  /**
   * E.g. with the lockTableSize being fixed to 10000, a partition (i.e. the
   * range that is assigned to a thread) is 2500 when we have 4 threads
   * */
  int partitionSize = lockTableSize / numThreads;

  for (int i = 1; i < numThreads;
       i++) {  // Note that when numThreads = 1, only
               // wait on numLocks (see on previous insert)
    waitOn.insert(base + partitionSize * i - 1);
  }

  /**
   *  Now the experiment starts: Transaction A acquires all locks in its lock
   * budget.
   *
   * We iterate over the range of locks in a specific way, because as you know
   * each thread gets assigned one partition of the lock table, one sequential
   * range of IDs, iterating over the IDs sequentially wouldn't equally
   * distribute the requests over the threads.
   */
  for (int base = 0; base < numLocks;
       base +=
       lockTableSize) {  // iterate over the multiples of the lock table size
    for (int i = 1; i <= partitionSize; i++) {
      for (int threadId = 0; threadId < numThreads;
           threadId++) {  // iterate over the thread partitions
        int rowId = base + threadId * partitionSize + i;

        /**
         * Usually clients waited until their lock request returns the
         * signature, so all requests from a single client would end up being
         * synchronous, so that never more than one request is in the job queue.
         * Therefore we added an additional mode of operation for all lock
         * requests where we are not waiting for the result. Therefore the
         * client will issue new requests when the former ones aren't finished
         * yet and requests have a chance to queue up and being operated on
         * concurrently.
         */
        if (waitOn.find(rowId) != waitOn.end()) {
          lockManager.lock(transactionA, rowId, false, true);
        } else {
          lockManager.lock(transactionA, rowId, false, false);
        }
      }
    }
  }

  /**
   * So does B, as B is last we wait on the specific RIDs computed previously. B
   * acquires the same locks as A, so locks that are potentially paged out, are
   * paged in again. We does this in an attempt to show the paging overhead in
   * Intel SGX enclaves.
   */
  for (int base = 0; base < numLocks;
       base +=
       lockTableSize) {  // iterate over the multiples of the lock table size
    for (int i = 1; i <= partitionSize; i++) {
      for (int threadId = 0; threadId < numThreads;
           threadId++) {  // iterate over the thread partitions
        int rowId = base + threadId * partitionSize + i;

        if (waitOn.find(rowId) != waitOn.end()) {
          lockManager.lock(transactionB, rowId, false, true);
        } else {
          lockManager.lock(transactionB, rowId, false, false);
        }
      }
    }
  }

  // Locks are released in the same manner as they are acquired.
  for (int base = 0; base < numLocks;
       base +=
       lockTableSize) {  // iterate over the multiples of the lock table size
    for (int i = 1; i <= partitionSize; i++) {
      for (int threadId = 0; threadId < numThreads;
           threadId++) {  // iterate over the thread partitions
        int rowId = base + threadId * partitionSize + i;

        if (waitOn.find(rowId) != waitOn.end()) {
          lockManager.unlock(transactionA, rowId, true);
          lockManager.unlock(transactionB, rowId, true);
        } else {
          lockManager.unlock(transactionA, rowId, false);
          lockManager.unlock(transactionB, rowId, false);
        }
      }
    }
  }
}

auto main() -> int {
  spdlog::set_level(spdlog::level::err);

  vector<vector<long>> contentCSVFile;
  for (int lockBudget :
       lockBudgets) {  // Show effect of increasing number of locks
    vector<long> durations;
    for (int i = 0; i < repetitions; i++) {  // To make result more stable
      auto lockManager = LockManager(numWorkerThreads);
      lockManager.registerTransaction(transactionA, lockBudget);
      lockManager.registerTransaction(transactionB, lockBudget);

      //=========== TIME MEASUREMENT ================
      auto begin = high_resolution_clock::now();
      experiment(lockManager, lockBudget, numWorkerThreads);
      auto end = high_resolution_clock::now();
      //=============================================

      long duration = duration_cast<nanoseconds>(end - begin).count();
      durations.push_back(duration);

      vector<long> rowInCSVFile = {numWorkerThreads, lockBudget, duration};
      contentCSVFile.push_back(rowInCSVFile);

      sleep_for(seconds(1));  // because unlock is asynchronous
      flushCache();
    }

    long time = reduce(durations.begin(), durations.end()) / durations.size();
    spdlog::error("Finished experiment for lock budget " +
                  std::to_string(lockBudget));
  }
  writeToCSV("out", contentCSVFile);
  return 0;
}