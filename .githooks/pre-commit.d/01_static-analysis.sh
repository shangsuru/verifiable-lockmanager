#!/usr/bin/env bash

# DEFAULTS
PROGNAME=$(basename $0)

# USAGE FUNCTION
function usage()
{
   cat << HEREDOC
   This script initializes the development environment for the specified system type.

   Usage: $PROGNAME --unstaged

   required arguments:
     -u, --unstaged    set flag to apply operation to unstaged files

HEREDOC
}

# Determine the modified files this looks only at staged files (--cached)
# This behaviour is intended for the git hook
CXX_CHANGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -Ee "\.(cpp|h|cc)$")

# ARGUMENTS
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -h|--help)
    usage
    exit 0
    ;;
    -u|--unstaged) # parameter with value
    # this looks at unstaged files, this is useful during development
    CXX_CHANGED_FILES=$(git diff --name-only --diff-filter=ACM | grep -Ee "\.(cpp|h|cc)$")
    shift # past argument
    shift # past value
    ;;
    *) shift ;;     # not used parameters
esac
done

# Perform static analysis of code, e.g., using clang-tidy
echo '[git hook] running clang-tidy on files'

# check if there are files to tidy
if [ -z "${CXX_CHANGED_FILES}" ]; then 
    exit 0
fi

# The following commands can be used to re-generate the base .clang-tidy file
#-dump-config; # in case we need to change the flags and create a new .clang-tidy
#-checks=-*,clang-diagnostic-*,clang-analyzer-*,bugprone*,modernize*,performance*,readability*,-modernize-pass-by-value,-modernize-use-auto;

# Set up a compile command database
# -DCMAKE_EXPORT_COMPILE_COMMANDS=ON will generate a compile_commands.json that will serve as a compile command database
# -S refers to the folder with the source files and -B to the build folder
# Do not use ninja since it generates realtive compile commands that run-clang-tidy cannot handle
BUILDFOLDER=build-tidy
cmake -S . -B ${BUILDFOLDER} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Running clang-tidy on the following files:"
echo ${CXX_CHANGED_FILES}
# We use clang-tidy instead of run-clang-tidy.py since it is more robust
# Use -export-fixes to be able to apply fixes at once and check if there is any work to do
clang-tidy -p ${BUILDFOLDER} -extra-arg=-Wno-unknown-warning-option -export-fixes=fixes.yaml ${CXX_CHANGED_FILES}
result=$?
if [[ -f "fixes.yaml" ]]; then
    echo -n "Apply changes ... "
    clang-apply-replacements .
    echo "Done"
    rm fixes.yaml
    # We have applied fixes that the developer need to stage => return non 0 exit code
    result=1
fi
rm -rf $BUILDFOLDER
exit $result