#!/bin/ash

# Get input
ft=$1
si=$2
itf=$3
phy=$4

# Get Channel List
chan=`awk -f Get_Channel.awk "${phy}"`

sleep 1

# Start Channel hop
for i in $(seq 1 1 $ft)
do
    for ch in ${chan}
    do
        echo "${itf} set Channel ${ch}"
        iw dev "${itf}" set channel "${ch}"
        sleep $si
    done
done

# Stop tcpdump capturing
pkill "tcpdump"

