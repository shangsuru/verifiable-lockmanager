num_threads=(8)
num_locks=(700000)
output_file=out.csv
sealed_keys_file=sealed_data_blob.txt

echo "Starting evaluation..."

# Delete old output file
if [ -f "$output_file" ]; then
    rm $output_file
fi

# Compile the project in release mode
cmake -DSGX_HW=ON -DSGX_MODE=Debug -DCMAKE_BUILD_TYPE=Release -S .. -B ../build >/dev/null

for thread in ${num_threads[*]}
do
  # Set number of threads
  sed -i -e "s/numWorkerThreads = [0-9]*/numWorkerThreads = ${thread}/" benchmark.cpp
  thread_num_config=$(($thread+2)) # two more for transaction table and main thread
  sed -i -e "s/<TCSNum>[0-9]*/<TCSNum>${thread_num_config}/" ../src/enclave/enclave.config.xml

  for locks in ${num_locks[*]}
  do

    # Set number of locks to acquire
    sed -i -e "s/lockBudget = [0-9]*/lockBudget = ${locks}/" benchmark.cpp
    # Set lock table size to the number of locks (to avoid collisions having an effect on the evaluation)
    sed -i -e "s/arg.lock_table_size = [0-9]*/arg.lock_table_size = ${locks}/" ../src/lockmanager/lockmanager.cpp

    # Build the project
    cmake --build ../build >/dev/null

    # Get most recent enclave.signed.so
    cp ../build/apps/enclave.signed.so .

    # Remove old sealed keys, they cannot be opened by the enclave when its config changed, throwing an error
    if [ -f "$sealed_keys_file" ]; then
      rm $sealed_keys_file
    fi

    # Start the benchmarking
    ./../build/evaluation/benchmark

    echo "Finished experiment with ${locks} locks and ${thread} threads"
  done
done

# Reset everything to its original values
sed -i -e "s/numWorkerThreads = [0-9]*/numWorkerThreads = 1/" benchmark.cpp
sed -i -e "s/lockBudget = [0-9]*/lockBudget = 10/" benchmark.cpp
sed -i -e "s/arg.lock_table_size = [0-9]*/arg.lock_table_size = 10000/" ../src/lockmanager/lockmanager.cpp
sed -i -e "s/<TCSNum>[0-9]*/<TCSNum>3/" ../src/enclave/enclave.config.xml

rm $sealed_keys_file
rm enclave.signed.so