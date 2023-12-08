#!/bin/ash

# Get input
ft=$1
si=$2
itf=$3
phy=$4
debug=$5

# Get Channel List
chan=`awk -f Get_Channel.awk "${phy}"`

# Wait for tcpdump do startup
sleep 1

# Start Channel hop
for i in $(seq 1 1 $ft)
do
    for ch in ${chan}
    do
        if [ ${debug} -eq 1 ]
        then
            echo "${itf} set Channel ${ch}"
        fi
        
        iw dev "${itf}" set channel "${ch}"
        sleep $si
    done
done

# Stop tcpdump capturing
pid=`ps | grep -E "tcpdump.*ieee802_11_radio.*${itf}" | grep -v "grep" | awk '{print $1}'`
kill ${pid}
