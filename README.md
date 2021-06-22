# TrustDBle Lockmanager

A Trusted Lock Manager, which allows the use of different isolation levels. The Lock Manager will run inside an Intel SGX Trusted Execution Environment to ensure that peers cannot manipulate the locking.

## Project Structure
All (sub)folders containing C++ code follow the project structure describe in this [book](https://cliutils.gitlab.io/modern-cmake/chapters/basics/structure.html) (please make sure to read it if you want to contribute).
```
├── .githooks/                      # Contains custom git hooks that we use for this repo, inspired by https://gist.github.com/damienrg/411f63a5120206bb887929f4830ad0d0
│   ├── pre-commit.d                # Scripts that are executed before a commit is preformed
│   │   ├── 01_static-analysis.sh   # Checks the code for coding best practices using clang-tidy
|   |   └── 02_code-formatting.sh   # Checks the formatting of the code before a commit
|   ├── pre-push.d                  # Scripts that are executed before code is pushed to the repository
│   │   ├── 01_doxygen.sh           # Generate the project documentation and report any missing documentation as errors
|   |   └── 02_sanitizer_check.sh   # Executes tests with address sanitizer enabled
|   ├── pre-commit                  # The actual hook, calls scripts in the corresponding folder
|   └── pre-push                    # The actual hook, calls scripts in the corresponding folder
├── .vscode/                        # Contains standard vscode settings, e.g., file associations
├── ci/                             # Contains scripts and assets that are used for the CI pipeline
├── cmake/ 
|   ├── test_macros.cmake           # Macro to register new tests with gtest/ctest
|   └── trustdble_configure.cmake   # Custom cmake flag/variable definitions to e.g., compile with ASAN
├── docs/                           
│   ├── CMakeLists.txt              # Configuration of Doxygen, overview documentation
│   ├── doxygen-awesome.css         # Custom stylesheet to format doxygen HTML output
|   └── ...                         # Several Markdown files containing documentation
├── include/                        # 
├── src/                            # 
├── tests/                          #
```

## Getting Started

1. Clone this repo with 
```
git clone git@bitbucket.org:trustdble/trustdble-adapters.git
```  
2. Install the githooks via
```
git config core.hookspath .githooks
```
3. Configure the build
```
# add -GNinja to build with Ninja instead of unix Makefiles
# add -DWITH_ASAN=TRUE to build with address sanitizer enabled
cmake -S . -B build
```
4. Build the code
```
cmake --build build
```
5. Run the tests or build the documentation by specifying the corresponding target
```
cmake --build build --target test | docs
```

## Requirements

### Requirements for build process
To build the code you need cmake and google tests (will be installed by CMake). Also, if you want to use Ninja do not forget to install it:
```
# For Linux
sudo apt-get -y install build-essential cmake ninja-build 
# For MacOs
brew install cmake ninja
```

To generate the documentation please install doxygen plantuml graphviz:
```
# For Linux
sudo apt-get -y install doxygen plantuml graphviz

# For MacOs
brew install doxygen plantuml graphviz
```

## Git Hooks
This project uses pre-commit and pre-push scripts to ensure the quality of code.
See the description under [Project Structure](#Project-Structure) for a short explanation of each hook.
The hooks are installed by running `git config core.hookspath .githooks`.

Use `git push/commit --dry-run` to run the hooks without actually executing the git command. Alternatively, run the hook script directly, e.g., `.githooks/pre-push.d/01_doxygen.sh` (Note some hook script might require you to stage files (`git add`) or use an additional flag).​

Use `git push/commit --no-verify` to skip the execution of hooks and only perform the corresponding git command (only do this if you know what are you doing).