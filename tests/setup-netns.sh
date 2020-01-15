#!/bin/bash

hostint="$1"

[[ -z "$hostint" ]] && hostint=$(ip -o -4 route show to default | awk '{print $5}')

###############################################
# Configure host iptables                     #
###############################################
echo 1 > /proc/sys/net/ipv4/ip_forward

# Flush forward rules, policy DROP by default.
iptables -P FORWARD DROP
iptables -F FORWARD

# Flush nat rules.
iptables -t nat -F

# Enable masquerading of 10.200.1.0.
iptables -t nat -A POSTROUTING -s 10.201.202.0/24 -o $hostint -j MASQUERADE

# Allow forwarding between $hostint and v-eth1.
iptables -A FORWARD -i $hostint -o v-eth1 -j ACCEPT
iptables -A FORWARD -o $hostint -i v-eth1 -j ACCEPT
###############################################
###############################################


###############################################
# Init namespace                              #
###############################################
ip netns del client1

ip netns add client1
ip netns exec client1 ip link set lo up
###############################################
###############################################


###############################################
# Set up tunnel for namespace                 #
###############################################
ip link del v-eth1 &>/dev/null
ip link add v-eth1 type veth peer name v-peer1
ip link set v-peer1 netns client1
ip addr add 10.201.202.1/24 dev v-eth1
ip link set v-eth1 up
###############################################
###############################################


###############################################
# Configure network in namespace              #
###############################################
ip netns exec client1 ip addr add 10.201.202.2/24 dev v-peer1
ip netns exec client1 ip link set v-peer1 up
ip netns exec client1 ip route add default via 10.201.202.1
###############################################
###############################################
