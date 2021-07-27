#pragma once

#include <iostream>

#include "sgx_error.h"

struct SGX_ErrorList
{
  sgx_status_t err; // Error type
  const char *msg;  // Error message for corresponding error type
};

/**
 * Pretty prints SGX error codes
 */
void ret_error_support(sgx_status_t ret);