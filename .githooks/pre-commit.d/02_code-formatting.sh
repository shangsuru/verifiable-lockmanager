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
REGEX="\.(cpp|h|cc)$"
CXX_CHANGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -Ee ${REGEX})

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
    CXX_CHANGED_FILES=$(git diff --name-only --diff-filter=ACM | grep -Ee ${REGEX})
    shift # past argument
    shift # past value
    ;;
    *) shift ;;     # not used parameters
esac
done

# Format code, e.g., using clang-format
echo '[git hook] running clang-format on staged files'

# check if there are files to tidy
if [ -z "${CXX_CHANGED_FILES}" ]; then 
    exit 0
fi

echo "Running clang-format on the following files:"
echo ${CXX_CHANGED_FILES}
# Check if there are files which formatting we need to change
clang-format --dry-run --Werror ${CXX_CHANGED_FILES}
if [ $? -ne 0 ]; then 
    echo -n "Format files ... "
    # Run clang-format using a .clang-format config file, do changes in place
    clang-format -i ${CXX_CHANGED_FILES}
    echo "Done"
    # We have applied fixes that the developer need to stage => return non 0 exit code
    exit 1
fi