#!/bin/ash

# Get input
ft=$1
si=$2
itf=$3
phy=$4
debug=$5

# Get Channel List
chan=`awk -f Get_Channel.awk "${phy}"`
logFile="logCS"

# Wait for tcpdump do startup
sleep 1

echo "---------channel_hop.sh---------" >> "${logFile}"
# Start Channel hop
for i in $(seq 1 1 $ft)
do
    for ch in ${chan}
    do
        cmd="${itf} set Channel ${ch}"
        if [ ${debug} -eq 1 ]; then
            #echo "${itf} set Channel ${ch}" | tee -a "${logFile}"
            echo "${cmd}" | tee -a "${logFile}"
        else
            #echo "${itf} set Channel ${ch}" >> "${logFile}"
            echo "${cmd}" >> "${logFile}"
        fi
        
        iw dev "${itf}" set channel "${ch}"
        sleep $si
    done
done

# Stop tcpdump capturing
pid=`ps | grep -E "tcpdump.*ieee802_11_radio.*${itf}" | grep -v "grep" | awk '{print $1}'`
kill ${pid}
echo "kill ${pid}" >> "${logFile}"
