#!/bin/bash

panid="0xbeef"


PHYNUM=`iwpan dev | grep -B 1 wpan0 | sed -ne '1 s/phy#\([0-9]\)/\1/p'`

ip netns delete wpan0
ip netns add wpan0
iwpan phy${PHYNUM} set netns name wpan0

ip netns exec wpan0 iwpan dev wpan0 set pan_id $panid
ip netns exec wpan0 ip link add link wpan0 name lowpan0 type lowpan
ip netns exec wpan0 ip link set wpan0 up
ip netns exec wpan0 ip link set lowpan0 up


