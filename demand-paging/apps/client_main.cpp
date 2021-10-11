#include "client.h"

void requestLocks(LockingServiceClient& client, unsigned int rowId) {
  client.requestSharedLock(1, rowId);
}

void RunClient() {
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  int transactionA = 1;
  int transactionB = 2;
  int lockBudget = 120;
  client.registerTransaction(transactionA, lockBudget);
  client.registerTransaction(transactionB, lockBudget);
  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId);
  }
  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestSharedLock(transactionB, rowId);
  }

  // Both release the locks again
  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId);
    client.requestUnlock(transactionB, rowId);
  }
}

auto main() -> int {
  spdlog::info("Starting client");
  RunClient();
  return 0;
}