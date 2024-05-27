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

chan=`awk -f Get_Current_Channel.awk ${phy}`

# Avoid multiple AP to scan at the same time
# sleep $delay

cd ~/cs/Monitor
# ./monitor.sh -m "${moni_itf}" -t "${target_itf}" -s "${si}" -w monitor

# Wait until monitor finish then do Scan
wait
# sleep 5

cd ~/cs/Scan
scan_result=`./Scan.sh -p "${phy}" -w scan`


cd ~/cs/Analysis
analysis_result=`./Analysis.sh "${scan_result}" $chan`

if [ $analysis_result == "0" ]; then
    echo "No better channel"
    echo "CS Finish"
    exit 0
fi

cd ~/cs/Decision
decision=`./Decision.sh "${analysis_result}" $chan`

if [ $chan -eq $decision ]; then
    echo "No switch"
else
    echo "Switch channel to $decision"
fi

