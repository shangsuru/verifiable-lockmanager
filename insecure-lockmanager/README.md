# Insecure Lock Manager

This version of the Lock Manager is implemented outside of Intel SGX and serves as a baseline. Comparing it to the other lock manager implementations, we can see the overhead caused by Intel SGX and other security features like signing.

## Build the Code

````
$ insecure-lockmanager: cmake -S . -B build
$ insecure-lockmanager: cmake --build build
````

## Start gRPC server and client separately

````
$ insecure-lockmanager: cd build/apps
$ apps: ./serverMain
$ apps: ./clientMain
````

## Run tests

````
$ insecure-lockmanager: cd build/tests
$ tests: ./lockmanager_test
````

Tip: You can use `--gtest_filter=*unlock` or `--gtest_repeat=100` when running test executables to filter for specific tests or run tests multiple times to test for concurrency related errors.

## Run performance measurement

````
$ insecure-lockmanager: cd evaluation
$ evaluation: ./evaluation.sh
````