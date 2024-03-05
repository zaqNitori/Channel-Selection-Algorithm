#!/bin/ash
#
# This script will set a sleep time
# And when it expire then will kill the monitor process
#

cd ~/Monitor

count=$1
target_addr=$2
logFile="logMonitor"

sleep $count

# Stop monitoring
pid=`ps | grep -E "tcpdump.*ieee802_11_radio.*${target_addr}" | grep -v "grep" | awk '{print $1}'`
kill ${pid}
echo "kill ${pid}" >> "${logFile}"
