#!/bin/ash
#
# This script will set a sleep time
# And when it expire then will kill the monitor process
#

cd ~/Monitor

count=$1
moni_itf=$2
logFile="logMonitor"

sleep $count

# Stop monitoring
pid=`ps | grep -E "tcpdump -ne -y ieee802_11_radio -i ${moni_itf} -e" | grep -v "grep" | awk '{print $1}'`
kill ${pid}
echo "kill ${pid}" >> "${logFile}"
