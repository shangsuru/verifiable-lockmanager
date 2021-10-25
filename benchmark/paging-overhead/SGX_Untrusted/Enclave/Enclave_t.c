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

static sgx_status_t SGX_CDECL sgx_region_read(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_region_read_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_region_read_t* ms = SGX_CAST(ms_region_read_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	struct region** _tmp_page = ms->ms_page;
	size_t _len_page = sizeof(struct region*);
	struct region** _in_page = NULL;

	CHECK_UNIQUE_POINTER(_tmp_page, _len_page);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_page != NULL && _len_page != 0) {
		if ( _len_page % sizeof(*_tmp_page) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		_in_page = (struct region**)malloc(_len_page);
		if (_in_page == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_page, _len_page, _tmp_page, _len_page)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}

	region_read(_in_page, ms->ms_size);
	if (_in_page) {
		if (memcpy_s(_tmp_page, _len_page, _in_page, _len_page)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

err:
	if (_in_page) free(_in_page);
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
	struct region** _tmp_page = ms->ms_page;
	size_t _len_page = sizeof(struct region*);
	struct region** _in_page = NULL;

	CHECK_UNIQUE_POINTER(_tmp_page, _len_page);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_page != NULL && _len_page != 0) {
		if ( _len_page % sizeof(*_tmp_page) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		_in_page = (struct region**)malloc(_len_page);
		if (_in_page == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_page, _len_page, _tmp_page, _len_page)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}

	region_write(_in_page, ms->ms_size);
	if (_in_page) {
		if (memcpy_s(_tmp_page, _len_page, _in_page, _len_page)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

err:
	if (_in_page) free(_in_page);
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
	struct region** _tmp_page = ms->ms_page;
	size_t _len_page = sizeof(struct region*);
	struct region** _in_page = NULL;

	CHECK_UNIQUE_POINTER(_tmp_page, _len_page);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_page != NULL && _len_page != 0) {
		if ( _len_page % sizeof(*_tmp_page) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		_in_page = (struct region**)malloc(_len_page);
		if (_in_page == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_page, _len_page, _tmp_page, _len_page)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}

	region_do_nothing(_in_page, ms->ms_size);
	if (_in_page) {
		if (memcpy_s(_tmp_page, _len_page, _in_page, _len_page)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

err:
	if (_in_page) free(_in_page);
	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv; uint8_t is_switchless;} ecall_table[3];
} g_ecall_table = {
	3,
	{
		{(void*)(uintptr_t)sgx_region_read, 0, 0},
		{(void*)(uintptr_t)sgx_region_write, 0, 0},
		{(void*)(uintptr_t)sgx_region_do_nothing, 0, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[3][3];
} g_dyn_entry_table = {
	3,
	{
		{0, 0, 0, },
		{0, 0, 0, },
		{0, 0, 0, },
	}
};


sgx_status_t SGX_CDECL write_o(int i, int size)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_write_o_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_write_o_t);
	void *__tmp = NULL;


	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_write_o_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_write_o_t));
	ocalloc_size -= sizeof(ms_write_o_t);

	ms->ms_i = i;
	ms->ms_size = size;
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

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
	
	status = sgx_ocall(1, ms);

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
	status = sgx_ocall(2, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

