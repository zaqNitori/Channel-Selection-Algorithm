#!/bin/ash

channel_24="1 2 3 4 5 6 7 8 9 10 11 12 13"
interface=""
sleep_interval=1
for_time=1

while getopts i:s:f: flag
do
    case "${flag}" in
        i) interface=${OPTARG};;
        s) sleep_interval=${OPTARG};;
        f) for_time=${OPTARG};;
    esac
done

if [ "${interface}" == "" ]; then
    echo "Please give interface!"
    exit 0
fi

ifconfig wlan0 down
ifconfig wlan0-2 down
iw dev "${interface}" set channel 14

for i in $(seq 1 1 ${for_time})
do
    for ch in ${channel_24}
    do
        echo "${interface} Setting channel ${ch}"
        iw dev "${interface}" set channel "${ch}"
        sleep "${sleep_interval}"
    done
done

pkill tcpdump

echo ""
echo "Channel Hopping Done!"
iw dev "${interface}" set channel 6
ifconfig wlan0 up
ifconfig wlan0-2 up
sleep 1

wifi