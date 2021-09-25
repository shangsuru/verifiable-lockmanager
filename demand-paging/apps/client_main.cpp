#include "client.h"

void requestLocks(LockingServiceClient& client, unsigned int rowId) {
  client.requestSharedLock(1, rowId);
}

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  unsigned int transaction_id = 1;
  const unsigned int default_lock_budget = 10;

  client.registerTransaction(transaction_id, default_lock_budget);

  int num_threads = 8;
  unsigned int row_id = 1;
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.push_back(std::thread(requestLocks, std::ref(client), row_id++));
  }

  for (int i = 0; i < num_threads; i++) {
    threads[i].join();
  }
}

auto main() -> int {
  spdlog::info("Starting client");
  RunClient();
  return 0;
}