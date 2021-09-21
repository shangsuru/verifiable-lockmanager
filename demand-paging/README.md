#  Lockmanager with demand paging

This project is a simple lock manager implementation inside the enclave. 
When the memory limits of the enclave are reached, the expensive default demand paging is used. 

## Developing and debugging inside the container
Make sure to install the Remote-Container extension of VSCode
Re-open the workspace in the devcontainer (command Remote-Container: Reopen in container)
1) Inside the container you should be able to build the code in SGX SIM mode:
`cmake -DSGX_HW=OFF -DSGX_MODE=Debug -DCMAKE_BUILD_TYPE=Debug -S . -B build`

2) Build the application: `cd build && make -j 5`

3) Start a debugging session from within VSCode