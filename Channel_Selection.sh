#!/bin/ash
#
# This script will call iwchan.awk first to get channel effect
# and then call frame scan process to capture the flow data information 
# and combine them to show a more precisely channel measurement.
#

logFile="logCS"
echo "----------Channel_Selection.sh----------" >> "${logFile}"

# Show help messages
function show_help() {
    echo "-d for debug mode. Default 0, set to 1 will show more msg."
    echo "-p to specify which frequency band want to use."
    echo "-s for scan interval."
    echo "-f for the recursive time."
    echo ""
}

check=`tcpdump --version`
res=$?

if [ $res -ne 0 ]; then
    echo "Please install tcpdump!"
    exit 0
fi

# Variable Setting
phy=""
si=1
ft=1
debug=0

# Get Setting From Option Arguments
while getopts i:s:f:p:d: flag
do
    case "${flag}" in
        p) phy=${OPTARG};;
        s) si=${OPTARG};;
        f) ft=${OPTARG};;
        d) debug=${OPTARG};;
        h) 
            show_help
            exit 0
            ;;
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
./Control_Interface.sh d "${non_monitor}" "${monitor}" "${waiting_chan}" "${debug}"

# Call frame_scan and channel_hop to capture the frames and will wait until both finish
./channel_hop.sh "${ft}" "${si}" "${monitor}" "${phy}" "${debug}" & frame_info=`awk -f frame_scan.awk "${phy}" "${monitor}"`
wait

# Resume Interfaces Settings after capturing the frames
./Control_Interface.sh u "${non_monitor}" "${monitor}" "${original_chan}" "${debug}"

# Call another awk script to combine the effect and frame_info data
awk -f Combine.awk "${effect}" "${frame_info}"

# Announce that this script is finished
echo "----------Channel_Selection.sh----------" >> "${logFile}"
echo "CS Finish!!" | tee -a "${logFile}"




