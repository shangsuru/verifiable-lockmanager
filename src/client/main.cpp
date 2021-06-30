#include "client.h"

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  LockingServiceClient client(
      grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  unsigned int transaction_id = 1;
  const unsigned int default_lock_budget = 10;
  unsigned int row_id = 2;

  client.registerTransaction(transaction_id, default_lock_budget);
  client.requestSharedLock(transaction_id, row_id);
}

auto main() -> int {
  std::cout << "Starting client" << std::endl;
  RunClient();
  return 0;
}