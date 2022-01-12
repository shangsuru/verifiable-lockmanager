# Verifiable Lockmanager

This project aims to build a Byzantine-fault tolerant pessimistic lock manager using 2PL. For security, the lock manager resides inside a Trusted Execution Environment.
The subfolders of this project showcase different kinds of implementations of this lock manager for evaluation purposes:

- Insecure Lockmanager: insecure implementation without Intel SGX
- Demand Paging Lockmanager: implementation with Intel SGX, where paging sets in when the memory limit of the enclave is reached
- Out-of-enclave Lockmanager: dynamically growing locktable is stored outside of the enclave, in unprotected memory, to avoid paging

Further resources:

- [El-Hindi, Muhammad, et al. "TrustDBle: Towards trustable shared databases." Third International Symposium on Foundations and Applications of Blockchain. 2020](https://scfab.github.io/2020/FAB2020_p7.pdf)

---

## Project Structure
All (sub)folders containing C++ code follow the project structure described in this [book](https://cliutils.gitlab.io/modern-cmake/chapters/basics/structure.html) (please make sure to read it if you want to contribute).
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

---

## Getting Started 

1. Clone this repo with 
```
git clone https://github.com/shangsuru/verifiable-lockmanager.git
```  
2. Install the githooks via
```
git config core.hookspath .githooks
```

3. Go into one of the subfolders representing the differentlock manager implementations for further instructions on how to compile and run the project. 

---

## Developing and debugging inside a Docker container

If your processor does not support Intel SGX, you can also run the code inside a Docker container. To be able to run the SGX code in simulation mode without special hardware, 
make sure to install the Remote-Containers extension of VSCode.
Re-open the workspace in the devcontainer (command `Remote-Containers: Open folder in container`).

1. Inside the container you should be able to build the code in SGX SIM mode:
   `cmake -DSGX_HW=OFF -DSGX_MODE=Debug -DCMAKE_BUILD_TYPE=Debug -S . -B build`

2. Build the application: `cd build && make -j 5`

3. Start a debugging session from within VSCode

---

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

---

## Git Hooks
This project uses pre-commit and pre-push scripts to ensure the quality of code.
See the description under [Project Structure](#Project-Structure) for a short explanation of each hook.
The hooks are installed by running `git config core.hookspath .githooks`.

Use `git push/commit --dry-run` to run the hooks without actually executing the git command. Alternatively, run the hook script directly, e.g., `.githooks/pre-push.d/01_doxygen.sh` (Note some hook script might require you to stage files (`git add`) or use an additional flag).​

Use `git push/commit --no-verify` to skip the execution of hooks and only perform the corresponding git command (only do this if you know what are you doing).

---

## Troubleshooting

- If any error, e.g. "error while loading shared libraries" occurs during the compilation step, running the command 
`source /opt/intel/sgxsdk/environment` 
(the exact path can differ) may solve the issue.

- If it can't open the enclave file, make sure it can find the file enclave.signed.so and maybe copy it in the same directory. 