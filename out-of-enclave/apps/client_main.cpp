#include "client.h"

void requestLocks(LockingServiceClient& client, unsigned int rowId) {
  client.requestSharedLock(1, rowId);
}

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  int transactionA = 1;
  int transactionB = 2;
  int lockBudget = 100000;
  client.registerTransaction(transactionA, lockBudget);
  client.registerTransaction(transactionB, lockBudget);
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, true);
  }
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestSharedLock(transactionB, rowId, true);
  }

  // Both release the locks again
  for (int rowId = 1; rowId <= lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId, false);
    client.requestUnlock(transactionB, rowId, false);
  }
}

auto main() -> int {
  spdlog::info("Starting client");
  RunClient();
  return 0;
}