# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.194.0/containers/cpp/.devcontainer/base.Dockerfile

# [Choice] Debian / Ubuntu version: debian-11, debian-10, debian-9, ubuntu-20.04, ubuntu-18.04
# focal corresponds to ubuntu-20.04
ARG VARIANT="ubuntu-20.04"
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}
# These variables are required after the FROM instruction
ARG DISTRO="ubuntu20.04-server"
ARG VERSION="2.14"
ARG BUILD="100.2"

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    gnupg \
    wget

# Add intel sgx repository
RUN echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' > /etc/apt/sources.list.d/intel-sgx.list
RUN wget -qO - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | apt-key add -
RUN apt-get update 

# Install prereqs
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    python

# Install intel sgx sdk
WORKDIR /opt/intel
RUN wget https://download.01.org/intel-sgx/sgx-linux/2.14/distro/${DISTRO}/sgx_linux_x64_sdk_${VERSION}.${BUILD}.bin
RUN chmod +x sgx_linux_x64_sdk_${VERSION}.${BUILD}.bin
RUN echo 'yes' | ./sgx_linux_x64_sdk_${VERSION}.${BUILD}.bin
