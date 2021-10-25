#ifndef ENCLAVE_U_H__
#define ENCLAVE_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_status_t etc. */


#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PRINT_DEFINED__
#define PRINT_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, print, (const char* string));
#endif
#ifndef PRINT_INT_DEFINED__
#define PRINT_INT_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, print_int, (int d));
#endif

sgx_status_t region_create(sgx_enclave_id_t eid);
sgx_status_t region_touch(sgx_enclave_id_t eid);
sgx_status_t region_read(sgx_enclave_id_t eid, int size);
sgx_status_t region_write(sgx_enclave_id_t eid, int size);
sgx_status_t region_do_nothing(sgx_enclave_id_t eid, int size);
sgx_status_t region_destroy(sgx_enclave_id_t eid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
