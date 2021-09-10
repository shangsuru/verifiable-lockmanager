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

  unsigned int i = 0;
  std::thread t1(requestLocks, std::ref(client), i++);
  std::thread t2(requestLocks, std::ref(client), i++);
  std::thread t3(requestLocks, std::ref(client), i++);
  std::thread t4(requestLocks, std::ref(client), i++);
  std::thread t5(requestLocks, std::ref(client), i++);
  std::thread t6(requestLocks, std::ref(client), i++);
  std::thread t7(requestLocks, std::ref(client), i++);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
  t7.join();
}

auto main() -> int {
  spdlog::info("Starting client");
  RunClient();
  return 0;
}