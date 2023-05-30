#!/bin/ash

channel_24="1 2 3 4 5 6 7 8 9 10 11 12 13"
ft=$1
si=$2
interface=$3

sleep 1

for i in $(seq 1 1 1)
do
    for ch in ${channel_24}
    do
        iw dev wlan0-1 set channel "${ch}"
        sleep 1
    done
done

#echo "${pid}"
#kill "${pid}"
pkill "tcpdump"

echo ""
echo "Channel Hopping Done!"
iw dev "${interface}" set channel 6
ifconfig wlan0 up
ifconfig wlan0-2 up
sleep 1

wifi