
#include <gtest/gtest.h>
/*
#include "client.h"
#include "server.h"

class ClientServerTest : public ::testing::Test {
 protected:
  void SetUp() override { serverThread = std::thread(startServer); };

  void TearDown() override { serverThread.join(); };

 public:
  static void startServer() {
    LockingServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051",
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());

    server->Wait();
  };

  static auto getClient() -> LockingServiceClient {
    return LockingServiceClient(grpc::CreateChannel(
        "0.0.0.0:50051", grpc::InsecureChannelCredentials()));
  };

  static auto getExpectedSignature(unsigned int transactionId,
                                   unsigned int rowId, unsigned int lockBudget,
                                   bool isExclusive = false) -> std::string {
    std::string signature =
        std::to_string(transactionId) + "-" + std::to_string(rowId);

    if (isExclusive) {
      signature += "X";
    } else {
      signature += "S";
    }

    signature += "-" + std::to_string(lockBudget);

    return signature;
  }

 private:
  std::thread serverThread_;
};

unsigned int transaction_id = 0;
unsigned int row_id = 0;
const unsigned int kLockBudget = 1;
auto client = ClientServerTest::getClient();

// Make lock request without registering
TEST(ServerTest, noRegister) {
  try {
    client.requestSharedLock(transactionId, rowId);
    FAIL() << true;
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request failed"));
  } catch (...) {
    FAIL() << true;
  }
};

// Registering twice
TEST(ServerTest, registerTwice) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_FALSE(client.registerTransaction(transactionId, lockBudget));
};

// Simple request for shared access
TEST(ServerTest, sharedAccess) {
  const int number_of_concurrent_requests = 5;
  for (int i = 0; i < number_of_concurrent_requests; i++) {
    client.registerTransaction(transactionId, lockBudget);

    EXPECT_EQ(client.requestSharedLock(transactionId, rowId),
              ClientServerTest::getExpectedSignature(transactionId, rowId,
                                                     lockBudget));
    transaction_id++;
  }
};

// Lock budget runs out
TEST(ServerTest, lockBudget) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(
      client.requestSharedLock(transactionId, rowId),
      ClientServerTest::getExpectedSignature(transactionId, rowId, lockBudget));
  row_id++;
  try {
    client.requestSharedLock(transactionId, rowId);
    FAIL() << true;
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request failed"));
  } catch (...) {
    FAIL() << true;
  }
};

// Simple request for exclusive access
TEST(ServerTest, exclusiveAccess) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(client.requestExclusiveLock(transactionId, rowId),
            ClientServerTest::getExpectedSignature(transactionId, rowId,
                                                   lockBudget, true));
}

// Upgrade
TEST(ServerTest, upgrade) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(
      client.requestSharedLock(transactionId, rowId),
      ClientServerTest::getExpectedSignature(transactionId, rowId, lockBudget));
  EXPECT_EQ(client.requestExclusiveLock(transactionId, rowId),
            ClientServerTest::getExpectedSignature(transactionId, rowId,
                                                   lockBudget, true));
};

// Unlock
TEST(ServerTest, unlock) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(
      client.requestSharedLock(transactionId, rowId),
      ClientServerTest::getExpectedSignature(transactionId, rowId, lockBudget));
  EXPECT_TRUE(client.requestUnlock(transactionId, rowId));
};

// Unlock twice
TEST(ServerTest, unlockTwice) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(
      client.requestSharedLock(transactionId, rowId),
      ClientServerTest::getExpectedSignature(transactionId, rowId, lockBudget));
  EXPECT_TRUE(client.requestUnlock(transactionId, rowId));
  EXPECT_TRUE(client.requestUnlock(transactionId, rowId));
};

// No shared while exclusive
TEST(ServerTest, noSharedWhileExclusive) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(
      client.requestSharedLock(transactionId, rowId),
      ClientServerTest::getExpectedSignature(transactionId, rowId, lockBudget));

  transactionId++;
  try {
    client.requestSharedLock(transactionId, rowId);
    FAIL() << true;
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request failed"));
  } catch (...) {
    FAIL() << true;
  }
};

// No exclusive while exclusive
TEST(ServerTest, noExclusiveWhileExclusive) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(client.requestExclusiveLock(transactionId, rowId),
            ClientServerTest::getExpectedSignature(transactionId, rowId,
                                                   lockBudget, true));

  transactionId++;
  try {
    client.requestExclusive(transactionId, rowId);
    FAIL() << true;
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request failed"));
  } catch (...) {
    FAIL() << true;
  }
};

// No exclusive while shared
TEST(ServerTest, noExclusiveWhileShared) {
  EXPECT_TRUE(client.registerTransaction(transactionId, lockBudget));
  EXPECT_EQ(
      client.requestSharedLock(transactionId, rowId),
      ClientServerTest::getExpectedSignature(transactionId, rowId, lockBudget));

  transactionId++;
  try {
    client.requestExclusive(transactionId, rowId);
    FAIL() << true;
  } catch (const std::domain_error& e) {
    EXPECT_EQ(e.what(), std::string("Request failed"));
  } catch (...) {
    FAIL() << true;
  }
};
*/