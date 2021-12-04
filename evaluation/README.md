# Evaluation

Within each lock manager implementation subfolder there is an evaluation folder with a `benchmark.cpp` file that contains the experiment code.
To run the experiments, there are `evaluation.sh` bash scripts that automatically configure necessary parameters and compile and run the code. The results get logged into an `out.csv` file. E.g., to start the experiment for the insecure lock manager:

````
$ trustdble-lockmanager: cd insecucure-lockmanager/evaluation
$ evaluation: ./evaluation.sh
````

The columns of the generated CSV file are the number of threads used, how many locks each (of the two) transactions acquires and the time in nanoseconds that it took to acquire those locks.

Resulting data that was used to create the plots in `/experiment-results/evaluation.ipynb`:

`insecure.csv` for the insecure lock manager, `demand_paging.csv` for the demand paging lock manager, `out_of_enclave.csv` for the out-of-enclave lockmanager and `untrusted.csv` for the lockmanager that accesses untrusted memory for the lock table without integrity verification. 

Additionally we measured peak stack and heap sizes in KB when acquiring a certain number of locks for the demand paging lock manager (`stack_heap_sizes_demand_paging.csv`) and for the out-of-enclave lock manager (`stack_heap_sizes_out_of_enclave.csv`). To measure peak stack and heap sizes as well, use SGX-GDB with the SGX emmt extension, e.g.

````
$ trustdble-lockmanager: cd demand-paging/build/evaluation
$ evaluation: sgx-gdb ./benchmark
(gdb) enable sgx_emmt
(gdb) run
````


# Paging Overhead

The paging overhead folder contains an experiment from [ShieldStore](https://github.com/cocoppang/ShieldStore). It measure memory access latencies without SGX (`Unsecure`), with Intel SGX on memory inside the enclave (`SGX_Enclave`) and with Intel SGX on unprotected memory outside of the enclave (`SGX_Unprotected`). It shows the performance overhead of paging memory in and out of the enclave when the memory limit of Intel SGX is reached. 

# Switchless

The switchless folder contains code from the [Intel SGX Linux SDK code samples](https://github.com/intel/linux-sgx/tree/master/SampleCode) on switchless calls, a performance optimization for ECALLS and OCALLS. It demonstrates that with that feature enabled, ECALLS and OCALLS run a lot faster.