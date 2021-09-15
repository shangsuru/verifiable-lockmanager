#include "lockmanager.h"

void print_info(const char *str) {
  spdlog::info("Enclave: " + std::string{str});
}

void print_error(const char *str) {
  spdlog::error("Enclave: " + std::string{str});
}

void print_warn(const char *str) {
  spdlog::warn("Enclave: " + std::string{str});
}