#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "client.h"

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::nanoseconds;
using std::this_thread::sleep_for;

using namespace std;

const size_t bigger_than_cachesize =
    20 * 1024 * 1024;  // Checked cache size with command 'lscpu | grep cache'
long* p = new long[bigger_than_cachesize];
int transactionA = 1;
int transactionB = 2;

void flushCache() {
  // When you want to "flush" cache.
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

auto getClient() -> LockingServiceClient {
  string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  return client;
}

/**
 * A acquires given number of locks in shared mode, then B acquires the same
 * locks in shared mode. This makes A to write lock objects into the lock table
 * and B read them again later one.
 */
void experiment(LockingServiceClient& client, int numberOfLocks) {
  for (int rowId = 1; rowId <= numberOfLocks; rowId++) {
    client.requestSharedLock(transactionA, rowId);
  }
  for (int rowId = 1; rowId <= numberOfLocks; rowId++) {
    client.requestSharedLock(transactionB, rowId);
  }

  // Both release the locks again
  for (int rowId = 1; rowId <= numberOfLocks - 1; rowId++) {
    client.requestUnlock(transactionA, rowId);
    client.requestUnlock(transactionB, rowId);
  }

  client.requestUnlock(transactionA, numberOfLocks);
  client.requestUnlock(transactionB, numberOfLocks,
                       true);  // wait on last unlock
}

/**
 * Since previously, clients waited until their lock request returns the
 * signature, all requests from a single client ended up being synchronous, so
 * there was never more than one request in the queue. Therefore we added an
 * additional mode of operation for all lock requests where we are not waiting
 * for the result. Therefore the client issues new requests when the former
 * aren't finished yet and requests have a chance to queue up and being operated
 * on concurrently.
 */
void experiment1Thread(LockingServiceClient& client, int lockBudget = 60000) {
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000) {  // last RIDs for each thread to wait on
      client.requestSharedLock(transactionB, rowId, true);
    } else {
      client.requestSharedLock(transactionB, rowId, false);
    }
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000) {  // last RIDs for each thread to wait on
      client.requestUnlock(transactionB, rowId, true);
    } else {
      client.requestUnlock(transactionB, rowId, false);
    }
  }
}

/**
 * For multiple client multiple thread scenario, we have to make sure that we
 * wait on the last request for each thread, e.g. when there are two threads we
 * need to make the last two requests synchronous so that we wait on everything
 * to be finished.
 */
void experiment2Threads(LockingServiceClient& client, int lockBudget = 60000) {
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000 ||
        rowId == 59999) {  // last RIDs for each thread to wait on
      client.requestSharedLock(transactionB, rowId, true);
    } else {
      client.requestSharedLock(transactionB, rowId, false);
    }
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000 ||
        rowId == 59999) {  // last RIDs for each thread to wait on
      client.requestUnlock(transactionB, rowId, true);
    } else {
      client.requestUnlock(transactionB, rowId, false);
    }
  }
}

void experiment4Threads(LockingServiceClient& client, int lockBudget = 60000) {
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000 || rowId == 59999 || rowId == 57499 ||
        rowId == 54999) {  // last RIDs for each thread to wait on
      client.requestSharedLock(transactionB, rowId, true);
    } else {
      client.requestSharedLock(transactionB, rowId, false);
    }
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000 || rowId == 59999 || rowId == 57499 ||
        rowId == 54999) {  // last RIDs for each thread to wait on
      client.requestUnlock(transactionB, rowId, true);
    } else {
      client.requestUnlock(transactionB, rowId, false);
    }
  }
}

void experiment8Threads(LockingServiceClient& client, int lockBudget = 60000) {
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000 || rowId == 59999 || rowId == 58749 || rowId == 57499 ||
        rowId == 56249 || rowId == 54999 || rowId == 53749 ||
        rowId == 52499) {  // last RIDs for each thread to wait on
      client.requestSharedLock(transactionB, rowId, true);
    } else {
      client.requestSharedLock(transactionB, rowId, false);
    }
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    if (rowId == 60000 || rowId == 59999 || rowId == 58749 || rowId == 57499 ||
        rowId == 56249 || rowId == 54999 || rowId == 53749 ||
        rowId == 52499) {  // last RIDs for each thread to wait on
      client.requestUnlock(transactionB, rowId, true);
    } else {
      client.requestUnlock(transactionB, rowId, false);
    }
  }
}

auto main() -> int {
  vector<vector<long>> contentCSVFile;
  // vector<int> lockBudgets = {10, 100, 500, 1000, 2500, 5000, 10000,  20000,
  // 50000, 100000, 150000, 200000};
  vector<int> lockBudgets = {60000};  // how many locks to acquire
  const int repetitions = 25;  // repeats the same experiments several times
  const int numWorkerThreads =
      8;  // this is just written to the CSV file and has no influence on the
          // actual number of worker threads. These need to be adapted
          // separately in the respective lockmanager.cpp file (search for
          // "arg.num_threads")
  spdlog::set_level(spdlog::level::err);
  auto client = getClient();

  for (int lockBudget :
       lockBudgets) {  // Show effect of increasing number of locks
    vector<long> durations;
    for (int i = 0; i < repetitions; i++) {  // To make result more stable
      client.registerTransaction(transactionA, lockBudget);
      client.registerTransaction(transactionB, lockBudget);
      auto begin = high_resolution_clock::now();

      experiment8Threads(client, lockBudget);

      auto end = high_resolution_clock::now();
      long duration = duration_cast<nanoseconds>(end - begin).count();
      durations.push_back(duration);

      vector<long> rowInCSVFile = {numWorkerThreads, lockBudget, duration};
      contentCSVFile.push_back(rowInCSVFile);

      sleep_for(nanoseconds(10000));  // because unlock is asynchronous
      flushCache();
    }

    long time = reduce(durations.begin(), durations.end()) / durations.size();
    cout << "The time for " << lockBudget << ": " << time << endl;
  }
  writeToCSV("out", contentCSVFile);
  return 0;
}