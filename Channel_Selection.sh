#!/bin/ash
#
# This script will call iwchan.awk first to get channel effect
# and then call frame scan process to capture the flow data amount 
# and combine them to show a more precisely channel measurement.
#

# Variable Setting
phy=""
si=1
ft=1

# Get Setting From Option Arguments
while getopts i:s:f:p: flag
do
    case "${flag}" in
        p) phy=${OPTARG};;
        s) si=${OPTARG};;
        f) ft=${OPTARG};;
    esac
done

# The p tag phy should be needed
if [ "${phy}" == "" ]; then
    echo "Please give phy!"
    exit 0
fi

# Call iwchan.awk first and then store its output
effect=`awk -f iwchan.awk show ${phy}`

# Get interfaces from specific phy
itf_conf=`awk -f Search_Interface.awk ${phy}`

# use awk to extract the string 
non_monitor=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[1]}'`
monitor=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[2]}'`
original_chan=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[3]}'`
waiting_chan=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[4]}'`

# Use the data above to down interfaces
./Control_Interface.sh d "${non_monitor}" "${monitor}" "${waiting_chan}"

# Call frame_scan and channel_hop to capture the frames and will wait until both finish
./channel_hop.sh "${ft}" "${si}" "${monitor}" "${phy}" & amount=`awk -f frame_scan.awk "${phy}" "${monitor}"`
wait

# Resume Interfaces Settings after capturing the frames
./Control_Interface.sh u "${non_monitor}" "${monitor}" "${original_chan}"

# Call another awk script to combine the effect and amount data
awk -f Combine.awk "${effect}" "${amount}"

# Announce that this script is finished
echo "CS Finish!!"




