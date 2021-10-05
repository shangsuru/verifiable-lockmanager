#include <fstream>
#include <iostream>
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

auto main() -> int {
  // RunClient();
  return 0;
}