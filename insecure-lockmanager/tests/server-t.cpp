
#include <gtest/gtest.h>

#include "server.h"

class ServerTest : public ::testing::Test {
 protected:
  auto registerTransaction(LockingServiceImpl &server) -> bool {
    registration_.set_transaction_id(transactionId_);

    Status status =
        server.RegisterTransaction(&context_, &registration_, &acceptance_);
    return status.ok();
  }

  auto getSharedLock(LockingServiceImpl &server) -> bool {
    request_.set_transaction_id(transactionId_);
    request_.set_row_id(rowId_);

    Status status = server.LockShared(&context_, &request_, &response_);
    return status.ok();
  }

  auto getExclusiveLock(LockingServiceImpl &server) -> bool {
    request_.set_transaction_id(transactionId_);
    request_.set_row_id(rowId_);

    Status status = server.LockExclusive(&context_, &request_, &response_);
    return status.ok();
  }

  auto unlock(LockingServiceImpl &server) -> bool {
    request_.set_transaction_id(transactionId_);
    request_.set_row_id(rowId_);

    Status status = server.Unlock(&context_, &request_, &response_);
    return status.ok();
  }

  // Mocks for the gRPC call parameters
  ServerContext context_;
  LockRequest request_;
  LockResponse response_;
  Registration registration_;
  Acceptance acceptance_;
  unsigned int transactionId_ = 0;
  unsigned int rowId_ = 0;
};

// Make lock request without registering
TEST_F(ServerTest, noRegister) {
  LockingServiceImpl server;
  EXPECT_FALSE(getSharedLock(server));
};

// Registering twice
TEST_F(ServerTest, registerTwice) {
  LockingServiceImpl server;
  EXPECT_TRUE(registerTransaction(server));
  EXPECT_FALSE(registerTransaction(server));
};

// Simple request for shared access
TEST_F(ServerTest, sharedAccess) {
  LockingServiceImpl server;

  const int requests = 5;
  for (int i = 0; i < requests; i++) {
    registerTransaction(server);
    EXPECT_TRUE(getSharedLock(server));
    transactionId_++;
  }
};

// Simple request for exclusive access
TEST_F(ServerTest, exclusiveAccess) {
  LockingServiceImpl server;
  registerTransaction(server);
  EXPECT_TRUE(getExclusiveLock(server));
}

// Upgrade
TEST_F(ServerTest, upgrade) {
  LockingServiceImpl server;
  registerTransaction(server);
  EXPECT_TRUE(getSharedLock(server));
  EXPECT_TRUE(getExclusiveLock(server));
};

// Unlock
TEST_F(ServerTest, unlock) {
  LockingServiceImpl server;
  registerTransaction(server);
  getSharedLock(server);
  EXPECT_TRUE(unlock(server));
};

// Unlock twice
TEST_F(ServerTest, unlockTwice) {
  LockingServiceImpl server;
  registerTransaction(server);
  getSharedLock(server);
  EXPECT_TRUE(unlock(server));
  EXPECT_TRUE(unlock(server));
};

// No shared while exclusive
TEST_F(ServerTest, noSharedWhileExclusive) {
  LockingServiceImpl server;
  registerTransaction(server);
  getExclusiveLock(server);
  transactionId_++;
  EXPECT_FALSE(getSharedLock(server));
};

// No exclusive while exclusive
TEST_F(ServerTest, noExclusiveWhileExclusive) {
  LockingServiceImpl server;
  registerTransaction(server);
  getExclusiveLock(server);
  transactionId_++;
  EXPECT_FALSE(getExclusiveLock(server));
};

// No exclusive while shared
TEST_F(ServerTest, noExclusiveWhileShared) {
  LockingServiceImpl server;
  registerTransaction(server);
  getSharedLock(server);
  transactionId_++;
  EXPECT_FALSE(getExclusiveLock(server));
};
