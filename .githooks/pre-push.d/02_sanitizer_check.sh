#!/usr/bin/env bash

# DEFAULTS
PROGNAME=$(basename $0)
COMPILER="g++"

# USAGE FUNCTION
function usage()
{
   cat << HEREDOC
   This script initializes the development environment for the specified system type.

   Usage: $PROGNAME --compiler COMPILER

   required arguments:
     -c, --compiler    The compiler to use

HEREDOC
}

# ARGUMENTS
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -h|--help)
    usage
    exit 0
    ;;
    -c|--compiler) # parameter with value
    COMPILER="$2"
    shift # past argument
    shift # past value
    ;;
    *) shift ;;     # not used parameters
esac
done

# Perform address analysis of code, e.g., using clang-tidy
echo '[git hook] running build with address sanitizier'

BUILDFOLDER=build-san
# Set up a compile command database
# -DCMAKE_BUILD_TYPE=asan will enable AddressSanitizer to detect memory leaks
# -S refers to the folder with the source files and -B to the build folder
cmake -S . -B ${BUILDFOLDER} -DCMAKE_CXX_COMPILER=${COMPILER} -DCMAKE_BUILD_TYPE=Debug -DWITH_ASAN=ON -DCMAKE_CXX_STANDARD_LIBRARIES="-lcurl"
# We have to build the tests
cmake --build ${BUILDFOLDER}
# Now we can trigger the test to run
cmake --build ${BUILDFOLDER} --target test -- ARGS="-R Stub* --output-on-failure"
result=$?

rm -rf ${BUILDFOLDER}
exit $result