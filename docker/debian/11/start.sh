#!/bin/sh
set -e
set -x

ip link add dummy0 type dummy
ip link set dummy0 up
ip link add dummy1 type dummy
ip link set dummy1 up
ip link add dummy2 type dummy
ip link set dummy2 up
ip link add dummy3 type dummy
ip link set dummy3 up

# ./trex-cfg
# ./dpdk_setup_ports.py --show
# ./t-rex-64 -i --cfg /etc/trex_cfg.yaml
