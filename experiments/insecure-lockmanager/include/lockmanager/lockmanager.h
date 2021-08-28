#pragma once

#include <iostream>
#include <libcuckoo/cuckoohash_map.hh>
#include <memory>
#include <stdexcept>
#include <string>

#include "lock.h"
#include "spdlog/spdlog.h"
#include "transaction.h"

using libcuckoo::cuckoohash_map;

/**
 * Process lock and unlock requests from the server. It manages a lock table,
 * where for each row ID it can store the corresponding lock object, which
 * contains information like type of lock and number of owners. It also manages
 * a transaction table, which maps from a transaction Id to the corresponding
 * transaction object, which holds information like the row IDs the transaction
 * has a lock on or the transaction's lock budget.
 *
 * It makes use of Intel SGX to have a secure enclave for signing the locks and
 * protecting its internal data structures.
 */
class LockManager {
 public:
  /**
   * Acquires a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be locked
   * @param requestedMode either shared for concurrent read access or exclusive
   * for sole write access
   * @throws std::domain_error, when transaction did not call
   * RegisterTransaction before or the given lock mode is unknown or when the
   * transaction makes a request for a look, that it already owns, makes a
   * request for a lock while in the shrinking phase
   */
  void lock(unsigned int transactionId, unsigned int rowId,
            Lock::LockMode requestedMode);

  /**
   * Releases a lock for the specified row
   *
   * @param transactionId identifies the transaction making the request
   * @param rowId identifies the row to be released
   * @throws std::domain_error, when the transaction didn't call
   * RegisterTransaction before or the lock to unlock does not exist
   */
  void unlock(unsigned int transactionId, unsigned int rowId);

 private:
  /**
   * Releases all locks the given transaction currently has.
   *
   * @param transaction the transaction to be aborted
   */
  void abortTransaction(const std::shared_ptr<Transaction>& transaction);

  cuckoohash_map<unsigned int, std::shared_ptr<Lock>> lockTable_;
  cuckoohash_map<unsigned int, std::shared_ptr<Transaction>> transactionTable_;
};