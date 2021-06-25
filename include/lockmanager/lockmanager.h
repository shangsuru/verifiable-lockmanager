#pragma once

#include <string_view>

#include "lock.h"

/**
 * A LockManager
 */
class LockManager {
 public:
  /**
   * Acquires a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param lockMode either shared for concurrent read access or exclusive for
   *                 sole write access
   */
  static void lock(unsigned int transactionId, unsigned int rowId,
                   Lock::LockMode lockMode);

  /**
   * Releases a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   */
  static void unlock(unsigned int transactionId, unsigned int rowId);

  /**
   * Signs a lock with the private key of the lock manager. The signature can be
   * returned to the client, so he can verify to the storage layer, that he
   * acquired the lock for the respective read or write access to a row.
   *
   * @param transactionId identifies the transaction holding the lock
   * @param rowId identifies the row the lock was acquired for
   * @param blockTimeout timeout counter that invalidates the signature after
   *                     the block number of the underlying blockchain in the
   *                     storage layer reaches the block timeout number
   * @returns the signature of the lock
   */
  static auto sign(unsigned int transactionId, unsigned int rowId,
                   unsigned int blockTimeout) -> std::string_view;

  /**
   * Tells for a given row, if it has a shared or exclusive lock.
   * It assumes that this function is only called on a row, that
   * definetly has a lock.
   *
   * @param rowId identifies the row
   * @returns the mode of the lock, either shared or exclusive
   */
  static auto getLockMode(unsigned int rowId) -> Lock::LockMode;

  /**
   * Checks if the given transaction has a lock on the specified row.
   *
   * @param transactionId identifies the transaction
   * @param rowId identifies the row
   * @returns true if it has lock, else false
   */
  static auto hasLock(unsigned int transactionId, unsigned int rowId) -> bool;

  /**
   * Releases all locks the given transaction currently has.
   *
   * @param transactionId identifies the transaction
   */
  static void deleteTransaction(unsigned int transactionId);

 private:
  int privateKey_ = 0;  // TODO
  // TODO: lockTable, transactionTable
};