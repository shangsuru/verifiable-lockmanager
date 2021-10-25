#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define ADD_ASSIGN_OVERFLOW(a, b) (	\
	((a) += (b)) < (b)	\
)


typedef struct ms_region_read_t {
	int ms_size;
} ms_region_read_t;

typedef struct ms_region_write_t {
	int ms_size;
} ms_region_write_t;

typedef struct ms_region_do_nothing_t {
	int ms_size;
} ms_region_do_nothing_t;

typedef struct ms_print_t {
	const char* ms_string;
} ms_print_t;

typedef struct ms_print_int_t {
	int ms_d;
} ms_print_int_t;

static sgx_status_t SGX_CDECL sgx_region_create(void* pms)
{
	sgx_status_t status = SGX_SUCCESS;
	if (pms != NULL) return SGX_ERROR_INVALID_PARAMETER;
	region_create();
	return status;
}

static sgx_status_t SGX_CDECL sgx_region_touch(void* pms)
{
	sgx_status_t status = SGX_SUCCESS;
	if (pms != NULL) return SGX_ERROR_INVALID_PARAMETER;
	region_touch();
	return status;
}

static sgx_status_t SGX_CDECL sgx_region_read(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_region_read_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_region_read_t* ms = SGX_CAST(ms_region_read_t*, pms);
	sgx_status_t status = SGX_SUCCESS;



	region_read(ms->ms_size);


	return status;
}

static sgx_status_t SGX_CDECL sgx_region_write(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_region_write_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_region_write_t* ms = SGX_CAST(ms_region_write_t*, pms);
	sgx_status_t status = SGX_SUCCESS;



	region_write(ms->ms_size);


	return status;
}

static sgx_status_t SGX_CDECL sgx_region_do_nothing(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_region_do_nothing_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_region_do_nothing_t* ms = SGX_CAST(ms_region_do_nothing_t*, pms);
	sgx_status_t status = SGX_SUCCESS;



	region_do_nothing(ms->ms_size);


	return status;
}

static sgx_status_t SGX_CDECL sgx_region_destroy(void* pms)
{
	sgx_status_t status = SGX_SUCCESS;
	if (pms != NULL) return SGX_ERROR_INVALID_PARAMETER;
	region_destroy();
	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv; uint8_t is_switchless;} ecall_table[6];
} g_ecall_table = {
	6,
	{
		{(void*)(uintptr_t)sgx_region_create, 0, 0},
		{(void*)(uintptr_t)sgx_region_touch, 0, 0},
		{(void*)(uintptr_t)sgx_region_read, 0, 0},
		{(void*)(uintptr_t)sgx_region_write, 0, 0},
		{(void*)(uintptr_t)sgx_region_do_nothing, 0, 0},
		{(void*)(uintptr_t)sgx_region_destroy, 0, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[2][6];
} g_dyn_entry_table = {
	2,
	{
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
	}
};


sgx_status_t SGX_CDECL print(const char* string)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_string = string ? strlen(string) + 1 : 0;

	ms_print_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_print_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(string, _len_string);

	if (ADD_ASSIGN_OVERFLOW(ocalloc_size, (string != NULL) ? _len_string : 0))
		return SGX_ERROR_INVALID_PARAMETER;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_print_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_print_t));
	ocalloc_size -= sizeof(ms_print_t);

	if (string != NULL) {
		ms->ms_string = (const char*)__tmp;
		if (_len_string % sizeof(*string) != 0) {
			sgx_ocfree();
			return SGX_ERROR_INVALID_PARAMETER;
		}
		if (memcpy_s(__tmp, ocalloc_size, string, _len_string)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_string);
		ocalloc_size -= _len_string;
	} else {
		ms->ms_string = NULL;
	}
	
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL print_int(int d)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_print_int_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_print_int_t);
	void *__tmp = NULL;


	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_print_int_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_print_int_t));
	ocalloc_size -= sizeof(ms_print_int_t);

	ms->ms_d = d;
	status = sgx_ocall(1, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

