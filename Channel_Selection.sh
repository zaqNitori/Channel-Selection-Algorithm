#!/bin/ash
#
# This script will call iwchan.awk first to get channel effect
# and then call frame scan process to capture the flow data amount 
# and combine them to show a more precisely channel measurement.
#

# Get output from iwchan and store it.
effect=`awk -f iwchan.awk show phy0`

# Then will call frame scan

interface=""
si=1
ft=1

while getopts i:s:f: flag
do
    case "${flag}" in
        i) interface=${OPTARG};;
        s) si=${OPTARG};;
        f) ft=${OPTARG};;
    esac
done

if [ "${interface}" == "" ]; then
    echo "Please give interface!"
    exit 0
fi

ifconfig wlan0 down
ifconfig wlan0-2 down
iw dev "${interface}" set channel 14

./channel_hop.sh "${ft}" "${si}" "${interface}" & amount=`awk -f frame_scan.awk "${interface}"`

wait

echo "Channel Effect:"
echo "${effect}"
echo "Frame Amount:"
echo "${amount}"


echo "CS Finish!!"




