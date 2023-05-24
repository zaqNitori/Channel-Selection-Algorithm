#!/bin/ash

channel_24="1 2 3 4 5 6 7 8 9 10 11 12 13 14"

if [[ $# -eq 0 ]]
    then
        echo "Please give interface!"
        exit 0
fi
interface="${1}"

ifconfig wlan0 down
ifconfig wlan0-2 down

for ch in ${channel_24}
do
    echo "${interface} Setting channel ${ch}"
    iw dev "${interface}" set channel "${ch}"
    sleep 1
done

pkill tcpdump

echo "\nChannel Hopping Done!"
iw dev "${interface}" set channel 6
ifconfig wlan0 up
ifconfig wlan0-2 up
sleep 1

wifi