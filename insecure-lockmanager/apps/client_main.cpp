#include "client.h"

void requestLocks(LockingServiceClient& client, unsigned int rowId) {
  client.requestSharedLock(1, rowId);
}

void RunClient() {
  LockingServiceClient client(
      grpc::CreateChannel("0.0.0.0:50051", grpc::InsecureChannelCredentials()));

  int transactionA = 10;
  int transactionB = 2;
  int lockBudget = 11000;
  client.registerTransaction(transactionA);
  client.registerTransaction(transactionB);
  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestSharedLock(transactionA, rowId, false);
  }
  for (int rowId = 1; rowId < lockBudget - 1; rowId++) {
    client.requestSharedLock(transactionB, rowId, false);
  }
  client.requestSharedLock(transactionB, lockBudget - 1, true);

  // Both release the locks again
  for (int rowId = 1; rowId < lockBudget; rowId++) {
    client.requestUnlock(transactionA, rowId);
    client.requestUnlock(transactionB, rowId);
  }
}

auto main() -> int {
  spdlog::info("Starting client");
  spdlog::set_level(spdlog::level::err);
  RunClient();
  return 0;
}