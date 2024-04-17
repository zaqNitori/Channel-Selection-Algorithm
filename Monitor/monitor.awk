#!/usr/bin/awk -f
#
# Use tcpdump to scan without channel hopping
#

function extract(str) {
    return substr(str, 2, length(str) - 2)
}

function Get_Flag_Cmd() {

    if(myflag == "t") {
        # Scan for specific target
        cmd = cmd" | grep \""target_addr"\""
    }
    else if(myflag == "v") {
        # Scan except specific target
        cmd = cmd" | grep -v \""target_addr"\""
    }
    else if(myflag == "b") {
        # Scan everything but seperate target and non-target
        cmd = cmd
    }
    else {
        # Scan everything
        cmd = cmd
    }

}

function Scan() {
    
    first_tsft = 0
    time_pass = 0

    total_amount = 0
    total_size = 0
    duration = 0

    cmd = "tcpdump -ne -y ieee802_11_radio -i "moni_itf" -e -B 100000"
    Get_Flag_Cmd()

    # Reading Result of Tcpdump
    while(cmd | getline) {

        # Check if our info exists
        pos = index($0, "!")
        if(pos == 0) continue

        # Extract TSFT
        # Check if str we get is tsft
        if(tsft ~ /^[0-9]+$/){
            tsft = substr($2, 1, length($2) - 2)
        }
        else {
            tsft = first_tsft
        }

        # Check if the record we parse is within the interval time 
        if(first_tsft == 0) {
            first_tsft = tsft + 0
        }
        else {
            time_pass = (tsft - first_tsft) * 1.0 / 1000000
        }
        
        # Check if exceed the interval time , then kill the tcpdump process
        if(time_pass > (interval + 0)) {
            break
        }

        # Extract type and size
        # awk starts from index 1
        split($0, tmp, "!")
        split(tmp[2], good)
        type = good[1]
        size = good[2]
        dura = good[3]

        # Counting the frame amounts
        total_amount += 1

        # Collecting the frame sizes
        total_size += size + 0

        # Collecting the duration
        duration += dura

        # Collecting Received Energy.
        {
            pos = index($0, "dBm")
            if(pos == 0) continue

            tmp = substr($0, pos - 4, 4)
            cnt = split(tmp, sig, " ")
            if(cnt == 1)
                dbm = (sig[1] + 0)
            else
                dbm = (sig[2] + 0)
            
            watt = 10 ^ (dbm / 10.0)
            joule += dura * watt
        }
    }
    close(cmd)
}

function Show() {

    # Get duration and calculate usage
    # Plus 0.5 to do rounding
    #usage = (duration * 100) / (1000000 * interval) + 0.5
    usage = (duration) / (10000 * interval) + 0.5

    # Calculate Average Watt
    # And compared with CS.sh's output
    # to see if our system's ug_sig is bigger than or close to the ug_sig received from the channel
    ug_sig = -100
    if(dura > 0)
        ug_sig = 10 * (log(joule / duration) / log(10))

    printf "%d, %d, %d, %d\n", total_amount, total_size, usage, ug_sig

}

BEGIN {
    moni_itf    = ARGV[1]
    target_addr = ARGV[2]
    interval    = ARGV[3]
    myflag      = ARGV[4]

    Scan()
    Show()
}
