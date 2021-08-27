#include "client.h"

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  unsigned int transaction_id = 1;
  const unsigned int default_lock_budget = 10;
  unsigned int row_id = 2;

  spdlog::info("Registering transaction with TXID " + std::to_string(transaction_id));
  client.registerTransaction(transaction_id, default_lock_budget);

  spdlog::info("Requesting shared lock for RID " + std::to_string(row_id));
  std::string signature = client.requestSharedLock(transaction_id, row_id);

  spdlog::info("Received signature: " + signature);
}

auto main() -> int {
  spdlog::info("Starting client");
  RunClient();
  return 0;
}