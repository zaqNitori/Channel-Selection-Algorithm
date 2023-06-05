#!/bin/ash

ft=$1
si=$2
itf=$3
phy=$4

chan=`awk -f Get_Channel.awk "${phy}"`

sleep 1

for i in $(seq 1 1 $ft)
do
    for ch in ${chan}
    do
        echo "${itf} set Channel ${ch}"
        iw dev "${itf}" set channel "${ch}"
        sleep $si
    done
done

pkill "tcpdump"

iw dev "${itf}" set channel 6
ifconfig wlan0 up
ifconfig wlan0-2 up
sleep 1

wifi