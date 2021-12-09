# Untrusted Lockmanager

This version of the lockmanager is only used inside the evaluation. It stores the locktable outside of the enclave, in untrusted memory. When it accesses the lock table, in contrast to the Out-of-Enclave Lockmanager, it does no integrity verification. The purpose of this is to show the performance difference between accessing the locktable from trusted (i.e. Demand Paging Lockmanager) vs. untrusted memory.

## Build the Code

````
$ untrusted: cmake -S . -B build
$ untrusted: cmake --build build
````

## Start gRPC server and client separately

````
$ untrusted: cd build/apps
$ apps: ./serverMain
$ apps: ./clientMain
````

## Run tests

````
$ untrusted: cd build/tests
$ tests: ./lockmanager_test
````

Tip: You can use `--gtest_filter=*unlock` or `--gtest_repeat=100` when running test executables to filter for specific tests or run tests multiple times to test for concurrency related errors.

## Run performance measurement

````
$ untrusted: cd evaluation
$ evaluation: ./evaluation.sh
````