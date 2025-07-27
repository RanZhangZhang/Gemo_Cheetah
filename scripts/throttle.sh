#!/bin/bash
######
# Taken from https://github.com/emp-toolkit/emp-readme/blob/master/scripts/throttle.sh
######

## replace DEV=lo with your card (e.g., eth0)
DEV=lo 
if [ "$1" == "del" ]
then
	sudo tc qdisc del dev $DEV root
fi

if [ "$1" == "lan" ]
then
sudo tc qdisc del dev $DEV root
## about 3Gbps/ 1Gbps in geom
sudo tc qdisc add dev $DEV root handle 1: tbf rate 1000mbit burst 100000 limit 10000
## about 0.3ms ping latency
sudo tc qdisc add dev $DEV parent 1:1 handle 10: netem delay 0.15msec
fi
if [ "$1" == "wan" ]
then
sudo tc qdisc del dev $DEV root
## about 400Mbps / 30Mbps in geom
sudo tc qdisc add dev $DEV root handle 1: tbf rate 30mbit burst 100000 limit 10000
## about 40ms ping latency / 30ms in geom
sudo tc qdisc add dev $DEV parent 1:1 handle 10: netem delay 15msec
fi
