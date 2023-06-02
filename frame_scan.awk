#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function Initial() {
    cmd = "cat channel.txt"
    while(cmd | getline) {
        phy = $1
        freq = $2
        chan = $3
        channel[phy, freq, "chan"] = chan
        channel[phy, chan, "freq"] = freq
        frame[phy, chan] = 0
    }
    close(cmd)
}

function Scan() {
    loop = 1
    phy = "2G"
    cmd = "tcpdump -ne -y ieee802_11_radio -i "interface" -v -t -s0 -e"
    while(cmd | getline) {
        pos = index($0, "MHz")
        if(pos == 0) continue
        freq = substr($0, pos-5, 4)
        chan = channel[phy, freq, "chan"]
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
    interface=ARGV[1]
    Initial()
    Scan()
    Show()
}