#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function extract(str) {
    return substr(str, 2, length(str) - 2)
}

function Initial() {
    
    # Search available channel by using iw phy command
    cmd="iw phy "phy" channels | grep \"*\" | grep -v \"disabled\""

    # Initial the array while reading the channel list
    while(cmd | getline) {

        freq = $2
        chan = extract($4)
        channel[phy, freq, "chan"] = chan
        channel[phy, chan, "freq"] = freq
        amount[phy, chan, "Total"] = 0
        amount[phy, chan, "Mgmt"] = 0
        amount[phy, chan, "Ctrl"] = 0
        amount[phy, chan, "Data"] = 0
        size[phy, chan, "Total"] = 0
        size[phy, chan, "Mgmt"] = 0
        size[phy, chan, "Ctrl"] = 0
        size[phy, chan, "Data"] = 0
    }
    close(cmd)
}

function Scan() {
    
    cmd = "tcpdump -ne -y ieee802_11_radio -i "interface" -v -t -s0 -e | grep \"!\""

    # Reading Result of Tcpdump
    while(cmd | getline) {
        pos = index($0, "MHz")
        if(pos == 0) continue

        # Extract the freq
        freq = substr($0, pos-5, 4)
        chan = channel[phy, freq, "chan"]
        if(chan == 14) continue

        # Extract type and size
        split($0, tmp, "!")
        split(tmp, good)
        type = good[1]
        size = good[2]

        # Counting the frame amounts
        amount[phy, chan, "Total"]++
        amount[phy, chan, type]++

        # Collecting the frame sizes
        size[phy, chan, "Total"] += size + 0
        size[phy, chan, type] += size + 0
    }
    close(cmd)
}

function Show() {

    for(f_subs in amount) {
        split(f_subs, f, SUBSEP)
        if(f[1] != phy) continue
        phy = f[1]
        chan = f[2]
        freq = channel[phy, chan, "freq"]

        # Get Frame Amount
        ta = amount[phy, chan, "Total"]
        ma = amount[phy, chan, "Mgmt"]
        ca = amount[phy, chan, "Ctrl"]
        da = amount[phy, chan, "Data"]
        
        # Get Frame Size
        ts = size[phy, chan, "Total"]
        ms = size[phy, chan, "Mgmt"]
        cs = size[phy, chan, "Ctrl"]
        ds = size[phy, chan, "Data"]

        printf "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d!", freq, chan, ta, ma, ca, da, ts, ms, cs, ds
    }

}

BEGIN {
    phy=ARGV[1]
    interface=ARGV[2]
    Initial()
    Scan()
    Show()
}