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

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  unsigned int transaction_id = 1;
  const unsigned int default_lock_budget = 100;

  client.registerTransaction(transaction_id, default_lock_budget);
  client.requestExclusiveLock(transaction_id, 1);
}

void hello(int arg) {
  std::cout << "Hello " << std::to_string(arg) << std::endl;
}

auto main() -> int {
  long duration = executeNTimes(hello, 15);
  std::cout << duration << " ns" << std::endl;
  return 0;
}