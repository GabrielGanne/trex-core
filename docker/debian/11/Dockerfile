# syntax=docker/dockerfile:1

FROM debian:stable

RUN apt-get update

RUN apt-get update
RUN apt-get -y install \
        bash \
        g++ \
        gcc \
        git \
        iproute2 \
        iputils-ping \
        kmod \
        libibverbs-dev \
        libmnl-dev \
        libzmq5 \
        netbase \
        net-tools \
        pciutils \
        procps \
        python3 \
        python3-distutils \
        strace \
        sudo \
        vim \
        zlib1g-dev

# create a symbolic link to accommodate for the renamed file in python 3.9.
WORKDIR /usr/lib/x86_64-linux-gnu/
RUN ln -s -f libc.a liblibc.a

RUN mkdir /sources
WORKDIR /sources
RUN git clone https://github.com/GabrielGanne/trex-core.git

WORKDIR /sources/trex-core/linux_dpdk
RUN ./b configure --prefix=/usr  --no-mlx --no-bnxt \
     && ./b build -j4 -v

LABEL RUN docker run \
    --privileged --cap-add=ALL \
    -v /mnt/huge:/mnt/huge \
    -v /lib/modules:/lib/modules:ro \
    -v /sys/bus/pci/devices:/sys/bus/pci/devices \
    -v /sys/devices/system/node:/sys/devices/system/node \
    -v /dev:/dev \
    --name NAME \
    -e NAME=NAME \
    -e IMAGE=IMAGE IMAGE

WORKDIR /sources/trex-core/scripts
COPY trex_cfg.yaml /etc/trex_cfg.yaml
COPY start.sh .
RUN chmod +x start.sh
CMD ["./start.sh"]
