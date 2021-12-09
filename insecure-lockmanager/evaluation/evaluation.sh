num_threads=(1 2 4 8)
num_locks=(10 100 500 1000 2500 5000 10000 20000 50000 100000 150000 200000 300000 500000 700000)

echo "Starting evaluation..."

# Delete old output file
output_file=out.csv
if [ -f "$output_file" ]; then
    rm $output_file
fi

# Comment out sections that are not supposed to be included in the evaluation (upgrading locks and checking if a lock is already owned)
sed -i "s@// Comment out for evaluation ->@/* // Comment out for evaluation ->@" ../src/lockmanager/lockmanager.cpp
sed -i "s@// <- Comment out for evaluation@*/ // <- Comment out for evaluation@" ../src/lockmanager/lockmanager.cpp

# Compile the project in release mode
cmake -DCMAKE_BUILD_TYPE=Release -S .. -B ../build >/dev/null

for thread in ${num_threads[*]}
do
  # Set number of threads
  sed -i -e "s/numWorkerThreads = [0-9]*/numWorkerThreads = ${thread}/" benchmark.cpp
  for locks in ${num_locks[*]}
  do

    # Set number of locks to acquire
    sed -i -e "s/lockBudget = [0-9]*/lockBudget = ${locks}/" benchmark.cpp
    

    # Build the project
    cmake --build ../build >/dev/null

    # Start the benchmarking
    ./../build/evaluation/benchmark
    
    echo "Finished experiment with ${locks} locks and ${thread} threads"
  done
done

# Reset everything to its original values
sed -i -e "s/numWorkerThreads = [0-9]*/numWorkerThreads = 1/" benchmark.cpp
sed -i -e "s/lockBudget = [0-9]*/lockBudget = 10/" benchmark.cpp
sed -i "s@/\* // Comment out for evaluation ->@// Comment out for evaluation ->@" ../src/lockmanager/lockmanager.cpp
sed -i "s@\*/ // <- Comment out for evaluation@// <- Comment out for evaluation@" ../src/lockmanager/lockmanager.cpp