#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "client.h"

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

auto executeNTimes(std::function<void(int)> func, int arg) -> long {
  int n = 10000;
  std::vector<long> durations;
  for (int i = 0; i < n; i++) {
    auto begin = std::chrono::high_resolution_clock::now();
    func(arg);
    auto end = std::chrono::high_resolution_clock::now();
    long duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin)
            .count() /
        arg;
    durations.push_back(duration);
    flushCache();
  }

  return std::reduce(durations.begin(), durations.end()) / durations.size();
}

void writeToCSV(std::string filename, std::vector<std::vector<double>> values) {
  std::ofstream file;
  file.open(filename);

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
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  return client;
}

void experiment(LockingServiceClient& client, int numberOfLocks) {
  // A acquires given number of locks in shared mode, then B acquires the same
  // locks in shared mode. This makes A to write lock objects into the lock
  // table and B read them again later one.
  for (int rowId = 1; rowId < numberOfLocks; rowId++) {
    client.requestSharedLock(transactionA, rowId);
  }
  for (int rowId = 1; rowId < numberOfLocks; rowId++) {
    client.requestSharedLock(transactionB, rowId);
  }

  // Both release the locks again
  for (int rowId = 1; rowId < numberOfLocks; rowId++) {
    client.requestUnlock(transactionA, rowId);
    client.requestUnlock(transactionB, rowId);
  }
}

auto main() -> int {
  auto client = getClient();
  int lockBudget = 1000;
  client.registerTransaction(transactionA, lockBudget);
  client.registerTransaction(transactionB, lockBudget);
  experiment(client, lockBudget);
  return 0;
}