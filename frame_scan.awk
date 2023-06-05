#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function Initial() {
    # Decide which channel list to read according to the input
    if(phy == "phy0")
        cmd = "cat channel2G.txt"
    else
        cmd = "cat channel5G.txt"

    # Initial the array while reading the channel list
    while(cmd | getline) {
        freq = $1
        chan = $2
        channel[phy, freq, "chan"] = chan
        channel[phy, chan, "freq"] = freq
        frame[phy, chan] = 0
    }
    close(cmd)
}

function Scan() {
    
    cmd = "tcpdump -ne -y ieee802_11_radio -i "interface" -v -t -s0 -e"

    # Reading Result of Tcpdump
    while(cmd | getline) {
        pos = index($0, "MHz")
        if(pos == 0) continue

        # Extract the freq
        freq = substr($0, pos-5, 4)
        chan = channel[phy, freq, "chan"]
        if(chan == 14) continue

        # Counting the frame amounts
        frame[phy, chan]++
    }
    close(cmd)
}

function Show() {

    for(f_subs in frame) {
        split(f_subs, f, SUBSEP)
        if(f[1] != phy) continue
        phy = f[1]
        chan = f[2]
        freq = channel[phy, chan, "freq"]
        num = frame[phy, chan]
        
        printf "%d,%d,%d!", freq, f[2], num
    }

}

BEGIN {
    phy=ARGV[1]
    interface=ARGV[2]
    Initial()
    Scan()
    Show()
}