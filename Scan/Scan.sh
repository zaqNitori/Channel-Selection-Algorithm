#!/bin/ash
#
# This script will call iwchan.awk first to get channel effect
# and then call frame scan process to capture the flow data information 
# and combine them to show a more precisely channel measurement.
#

path=`pwd`
cd "${path}"
logFile="logCS"
echo "----------Channel_Selection.sh----------" >> "${logFile}"

# Show help messages
function show_help() {
    echo "-d for debug mode. Default 0, set to 1 will show more msg."
    echo "-p to specify which frequency band want to use."
    echo "-s for scan interval."
    echo "-f for the recursive time."
    echo "-w write output into file."
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
writeFile=""
writeFlag=0

# Get Setting From Option Arguments
while getopts s:f:p:w:dh flag
do
    case "${flag}" in
        p) phy=${OPTARG};;
        s) si=${OPTARG};;
        f) ft=${OPTARG};;
        d) debug=1;;
        w) 
            writeFile=${OPTARG}
            writeFlag=1
            ;;
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

#now=$(date +%H:%M)
#weekday=$(date +%a)
now=$(date +"%Y-%m-%d %H:%M")

# Call iwchan.awk first and then store its output
# effect=`awk -f iwchan.awk show ${phy}`

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
./channel_hop.sh "${ft}" "${si}" "${monitor}" "${phy}" "${debug}" & frame_info=`awk -f frame_scan.awk "${phy}" "${monitor}" "${si}" "${debug}"`
wait

# Resume Interfaces Settings after capturing the frames
./Control_Interface.sh u "${non_monitor}" "${monitor}" "${original_chan}" "${debug}"

# Call another awk script to combine the effect and frame_info data
# result=`awk -f Combine.awk "${effect}" "${frame_info}" "${debug}"`
result=`awk -f Combine.awk "${frame_info}" "${debug}"`

if [ $writeFlag -eq 1 ]; then
    # echo "${now}" > "${writeFile}"
    echo "${result}" | tee -a "${writeFile}"
else
    echo "${result}"
fi

# Announce that this script is finished
echo "Scan Finish!!" >> "${logFile}"
echo "----------End Scan.sh----------" >> "${logFile}"




