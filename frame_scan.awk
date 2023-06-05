#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function Initial() {
    if(phy == "phy0")
        cmd = "cat channel2G.txt"
    else
        cmd = "cat channel5G.txt"

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
    while(cmd | getline) {
        pos = index($0, "MHz")
        if(pos == 0) continue
        freq = substr($0, pos-5, 4)
        chan = channel[phy, freq, "chan"]
        if(chan == 14) continue
        frame[phy, chan]++
    }
    close(cmd)
}

function Show() {

    cmd = "sort -n"
    for(f_subs in frame) {
        split(f_subs, f, SUBSEP)
        if(f[1] != "2G") continue
        phy = f[1]
        chan = f[2]
        freq = channel[phy, chan, "freq"]
        num = frame[phy, chan]
        #printf "%d[%d] => %d\n", freq, f[2], num | cmd
        printf "%d,%d,%d!", freq, f[2], num
    }

    #close(cmd)
}

BEGIN {
    phy=ARGV[1]
    interface=ARGV[2]
    Initial()
    Scan()
    Show()
}