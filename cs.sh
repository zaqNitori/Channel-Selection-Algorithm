#!/bin/ash
#
# Whole CS step will be controlled by this script
# So far, will do Monitor before CS, so we can compare their result
#


# input 
phy=$1
moni_itf=$2
target_itf=$3
si=$4
delay=$5

# Avoid multiple AP to scan at the same time
sleep $delay

cd ~/cs/Monitor
./monitor.sh -m "${moni_itf}" -t "${target_itf}" -s "${si}" -w monitor

# Wait until monitor finish then do Scan
wait
sleep 5

cd ~/cs/Scan
./Scan.sh -p "${phy}" -w scan

