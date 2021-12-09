# Out-of-enclave Lockmanager

To avoid the overhead of EPC paging that occurs in the demand paging lock manager when the memory limit of the enclave is reached, this implementation stores the dynamically growing lock table outside of the enclave, avoiding the paging overhead inside the enclave. 

However, since the table now resides in unprotected memory, additional integrity verification is necessary when the enclave processes the table, as a sytem-level adversary could have modified its contents. 

Therefore, the enclave computes cryptographic hashes over the lock table and stores them inside of the enclave. Before processing content of the lock table, it will recompute the hash and compare it to the previously computed hash stored inside the enclave. When the hashes are equal, the enclave knows that the lock table has not been tampered with.

## Build the Code

````
$ out-of-enclave: cmake -S . -B build
$ out-of-enclave: cmake --build build
````

## Start gRPC server and client separately

````
$ out-of-enclave: cd build/apps
$ apps: ./serverMain
$ apps: ./clientMain
````

## Run tests

````
$ out-of-enclave: cd build/tests
$ tests: ./lockmanager_test
````

Tip: You can use `--gtest_filter=*unlock` or `--gtest_repeat=100` when running test executables to filter for specific tests or run tests multiple times to test for concurrency related errors.

## Run performance measurement

````
$ out-of-enclave: cd evaluation
$ evaluation: ./evaluation.sh
````