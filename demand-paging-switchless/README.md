# Lockmanager with standard EPC paging

This project is a simple lock manager implementation that completely resides inside the enclave.
When the memory limits of the enclave are reached, i.e. when a certain number of locks is acquired 
as their metadata is stored inside the enclave, EPC pages have to be paged in and out of trusted memory.

Typically OS’s support paging, which includes some overhead. However, the overhead associated
with paging in Intel SGX is greater, as described below:

Intel SGX uses secure storage, called the Enclave Page Cache (EPC), to stored enclave contents. Enclave pages are 4KB
in size. When enclaves are larger than the total memory available to the EPC, enclave paging can be used by privileged
SW to evict some pages to swap in other pages. The CPU performs the following steps using the EWB instruction when
the OS swaps out an enclave page:

- Read the SGX page to be swapped out (evicted)
- Encrypt the contents of that page
- Write the encrypted page to unprotected system memory

Since this process has an inherent overhead associated with it, the more pages that are swapped out, the more often
the overhead is incurred.