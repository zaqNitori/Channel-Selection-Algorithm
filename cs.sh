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

# Avoid multiple AP to scan at the same time
sleep 50

cd ~/cs/Monitor
./monitor.sh moni_itf target_itf si

# Wait until monitor finish then do Scan
wait
sleep 5

cd ~/cs/Scan
./Scan.sh -p "${phy}" -w scan

