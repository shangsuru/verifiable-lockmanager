#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_region_read_t {
	struct region** ms_page;
	int ms_size;
} ms_region_read_t;

typedef struct ms_region_write_t {
	struct region** ms_page;
	int ms_size;
} ms_region_write_t;

typedef struct ms_region_do_nothing_t {
	struct region** ms_page;
	int ms_size;
} ms_region_do_nothing_t;

typedef struct ms_write_o_t {
	int ms_i;
	int ms_size;
} ms_write_o_t;

typedef struct ms_print_t {
	const char* ms_string;
} ms_print_t;

typedef struct ms_print_int_t {
	int ms_d;
} ms_print_int_t;

static sgx_status_t SGX_CDECL Enclave_write_o(void* pms)
{
	ms_write_o_t* ms = SGX_CAST(ms_write_o_t*, pms);
	write_o(ms->ms_i, ms->ms_size);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_print(void* pms)
{
	ms_print_t* ms = SGX_CAST(ms_print_t*, pms);
	print(ms->ms_string);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_print_int(void* pms)
{
	ms_print_int_t* ms = SGX_CAST(ms_print_int_t*, pms);
	print_int(ms->ms_d);

	return SGX_SUCCESS;
}

static const struct {
	size_t nr_ocall;
	void * table[3];
} ocall_table_Enclave = {
	3,
	{
		(void*)Enclave_write_o,
		(void*)Enclave_print,
		(void*)Enclave_print_int,
	}
};
sgx_status_t region_read(sgx_enclave_id_t eid, struct region** page, int size)
{
	sgx_status_t status;
	ms_region_read_t ms;
	ms.ms_page = page;
	ms.ms_size = size;
	status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
	return status;
}

sgx_status_t region_write(sgx_enclave_id_t eid, struct region** page, int size)
{
	sgx_status_t status;
	ms_region_write_t ms;
	ms.ms_page = page;
	ms.ms_size = size;
	status = sgx_ecall(eid, 1, &ocall_table_Enclave, &ms);
	return status;
}

sgx_status_t region_do_nothing(sgx_enclave_id_t eid, struct region** page, int size)
{
	sgx_status_t status;
	ms_region_do_nothing_t ms;
	ms.ms_page = page;
	ms.ms_size = size;
	status = sgx_ecall(eid, 2, &ocall_table_Enclave, &ms);
	return status;
}

