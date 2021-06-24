#include <lockmanager.h>

#include <iostream>

void LockManager::lock(unsigned int transactionId, unsigned int rowId,
                       Lock::LockMode lockMode) {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
};

void LockManager::unlock(unsigned int transactionId, unsigned int rowId) {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
};

auto LockManager::sign(unsigned int transactionId, unsigned int rowId,
                       unsigned int blockTimeout) -> std::string_view {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  return "";
};

auto LockManager::getLockMode(unsigned int rowId) -> Lock::LockMode {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  return Lock::LockMode::kExclusive;
};

auto LockManager::hasLock(unsigned int transactionId, unsigned int rowId)
    -> bool {
  std::cout << __FUNCTION__ << " not yet implemented" << std::endl;
  return true;
};

void LockManager::deleteTransaction(unsigned int transactionId) {
  std::cout << __FUNCTION__ << " yet implemented" << std::endl;
};