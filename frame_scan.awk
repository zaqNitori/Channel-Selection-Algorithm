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

        freq                       = $2
        chan                       = extract($4)

        if(phy == "phy0" && (chan+0) > 11)
            continue
        if(phy == "phy1" && (chan+0) > 48)
            continue

        freq2chan[phy, freq]       = chan
        chan2freq[phy, chan]       = freq
        amount[phy, chan, "Total"] = 0
        amount[phy, chan, "Mgmt"]  = 0
        amount[phy, chan, "Ctrl"]  = 0
        amount[phy, chan, "Data"]  = 0
        size[phy, chan, "Total"]   = 0
        size[phy, chan, "Mgmt"]    = 0
        size[phy, chan, "Ctrl"]    = 0
        size[phy, chan, "Data"]    = 0
        duration[phy, chan]        = 0
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
        chan = freq2chan[phy, freq]
        if(chan == 14) continue

        # Extract type and size
        # awk starts from index 1
        split($0, tmp, "!")
        split(tmp[2], good)
        type = good[1]
        size = good[2]
        dura = good[3]

        # Counting the frame amounts
        amount[phy, chan, "Total"]++
        amount[phy, chan, type]++

        # Collecting the frame sizes
        size[phy, chan, "Total"] += size + 0
        size[phy, chan, type] += size + 0

        # Collecting the duration
        duration[phy, chan] += dura
    }
    close(cmd)
}

function Show() {

    for(subs in chan2freq) {
        split(subs, tmp, SUBSEP)
        
        phy = tmp[1]
        chan = tmp[2]
        freq = chan2freq[phy, chan]

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

        # Get duration and calculate usage
        # Plus 0.5 to do rounding
        dura = duration[phy, chan]
        usage = (dura * 100) / (1000000 * interval) + 0.5

        printf "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d!", freq, chan, ta, ts, ma, ms, ca, cs, da, ds, usage
    }

}

BEGIN {
    phy       = ARGV[1]
    interface = ARGV[2]
    interval  = ARGV[3]

    Initial()
    Scan()
    Show()
}