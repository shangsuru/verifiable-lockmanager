#ifndef ENCLAVE_T_H__
#define ENCLAVE_T_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include "sgx_edger8r.h" /* for sgx_ocall etc. */

#include "../Include/userdef.h"

#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

void region_read(struct region** page, int size);
void region_write(struct region** page, int size);
void region_do_nothing(struct region** page, int size);

sgx_status_t SGX_CDECL write_o(int i, int size);
sgx_status_t SGX_CDECL print(const char* string);
sgx_status_t SGX_CDECL print_int(int d);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
