#!/bin/ash
#
# This script will call iwchan.awk first to get channel effect
# and then call frame scan process to capture the flow data amount 
# and combine them to show a more precisely channel measurement.
#

interface=""
phy=""
si=1
ft=1

while getopts i:s:f:p: flag
do
    case "${flag}" in
        p) phy=${OPTARG};;
        i) interface=${OPTARG};;
        s) si=${OPTARG};;
        f) ft=${OPTARG};;
    esac
done

if [ "${phy}" == "" ]; then
    echo "Please give phy!"
    exit 0
fi

# Get output from iwchan and store it.
effect=`awk -f iwchan.awk show ${phy}`


itf_conf=`awk -f Search_Interface.awk ${phy}`
non_monitor=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[1]}'`
monitor=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[2]}'`
original_chan=`echo ${itf_conf} | awk '{split($0, s, "!"); print s[3]}'`

./Control_Interface.sh d "${non_monitor}" "${monitor}" 14

./channel_hop.sh "${ft}" "${si}" "${monitor}" "${phy}" & amount=`awk -f frame_scan.awk "${phy}" "${monitor}"`
wait

./Control_Interface.sh u "${non_monitor}" "${monitor}" "${original_chan}"

awk -f Combine.awk "${effect}" "${amount}"

echo "CS Finish!!"




