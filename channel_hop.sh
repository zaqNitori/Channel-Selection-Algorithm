#!/bin/ash

channel_24="1 2 3 4 5 6 7 8 9 10 11 12 13"
ft=$1
si=$2
itf=$3

sleep 1

for i in $(seq 1 1 $ft)
do
    for ch in ${channel_24}
    do
        echo "${itf} set Channel ${ch}"
        iw dev "${itf}" set channel "${ch}"
        sleep $si
    done
done

pkill "tcpdump"

