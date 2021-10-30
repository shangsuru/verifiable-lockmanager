#include "client.h"

void requestLocks(LockingServiceClient& client, unsigned int rowId) {
  client.requestSharedLock(1, rowId);
}

void RunClient() {
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  int transactionA = 1;
  int transactionB = 2;
  int lockBudget = 10000;
  client.registerTransaction(transactionA, lockBudget);
  client.registerTransaction(transactionB, lockBudget);

  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, false);
  }

  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestSharedLock(transactionB, rowId, false);
  }

  client.requestSharedLock(transactionA, lockBudget, true);
  client.requestSharedLock(transactionB, lockBudget, true);

  // Both release the locks again
  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId, false);
    client.requestUnlock(transactionB, rowId, false);
  }

  client.requestUnlock(transactionA, lockBudget, false);
  client.requestUnlock(transactionB, lockBudget, true);
}

auto main() -> int {
  spdlog::set_level(spdlog::level::info);
  spdlog::info("Starting client");
  RunClient();
  return 0;
}