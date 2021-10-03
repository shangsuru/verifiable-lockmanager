#include "client.h"

void requestLocks(LockingServiceClient& client, unsigned int rowId) {
  client.requestSharedLock(1, rowId);
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
  spdlog::info("Starting client");
  RunClient();
  return 0;
}