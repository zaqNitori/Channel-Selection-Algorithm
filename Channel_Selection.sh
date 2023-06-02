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

# Set same phy interface down so we can easily switch channel 
ifconfig wlan0 down
ifconfig wlan0-2 down
iw dev "${interface}" set channel 14

# Call channel_hop and frame_scan to capture frame amount
./channel_hop.sh "${ft}" "${si}" "${interface}" & amount=`awk -f frame_scan.awk "${interface}"`
wait

# After frame_scan then set config back for normal use
iw dev "${itf}" set channel 6
ifconfig wlan0 up
ifconfig wlan0-2 up
sleep 1

# Let the services reload the config
wifi

# Call another awk script to format the data
awk -f Combine.awk "${effect}" "${amount}"

echo "CS Finish!!"




