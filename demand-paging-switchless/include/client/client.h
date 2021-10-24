#pragma once

#include <grpcpp/grpcpp.h>

#include <iostream>
#include <string_view>

#include "lockmanager.grpc.pb.h"
#include "spdlog/spdlog.h"

#define target_address "0.0.0.0:50051"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

/**
 * The LockingServiceClient is the gRPC client for the lock manager.
 * It sends request to the gRPC server to acquire and release locks.
 */
class LockingServiceClient {
 public:
  /**
   * Initializes a new client stub, which provides the functions for
   * the gRPC calls to the server.
   *
   * @param channel the abstraction of a connection to the remote server
   */
  LockingServiceClient(const std::shared_ptr<Channel> &channel);

  /**
   * Registers the transaction at the lock manager by setting the lock budget.
   *
   * @param transactionId identifies the transaction
   * @param lockBudget the maximum number of locks the transaction can acquire
   * @returns if the registration was successful
   */
  auto registerTransaction(unsigned int transactionId, unsigned int lockBudget)
      -> bool;

  /**
   * Requests a shared lock for read-only access to a row.
   *
   * @param transactionId identifies the transaction that makes the request
   * @param rowId identifies the row, the transaction wants to access
   * @returns the signature of the lock
   * @throws std::domain_error, if the lock couldn't get acquired
   */
  auto requestSharedLock(unsigned int transactionId, unsigned int rowId)
      -> std::string;

  /**
   * Requests an exclusive lock for sole write access to a row.
   *
   * @param transactionId identifies the transaction that makes the request
   * @param rowId identifies the row, the transaction wants to access
   * @returns the signature of the lock
   * @throws std::domain_error, if the lock couldn't get acquired
   */
  auto requestExclusiveLock(unsigned int transactionId, unsigned int rowId)
      -> std::string;

  /**
   * Requests to release a lock acquired by the transaction.
   *
   * @param transactionId identifies the transaction that makes the request
   * @param rowId identifies the row, the transaction wants to unlock
   * @returns if the lock got released successfully
   */
  auto requestUnlock(unsigned int transactionId, unsigned int rowId) -> bool;

 private:
  std::unique_ptr<LockingService::Stub> stub_;
};
