#!/usr/bin/awk -f
#
# Combine output data from both scanning file
#

function seperate_Effect() {
    # Split the original String to get each channel
    n = split(effect, ce, "!")

    # for loop the array
    for( i = 1; i < n; i++) {
        # split each channel string to get effect data
        split(ce[i], s, ",")

        # create freq and channel mapping
        freq2chan[s[1]] = s[2]

        # store effect values and can use freq + chan to get them
        out[s[1], s[2], "effect"] = s[3]
    }

}

function seperate_FrameInfo() {
    # Split the original String to get each channel
    n = split(amount, tmp, "!")

    # for loop the array
    for( i = 1; i < n; i++) {
        
        # split each channel string to get amount data
        split(tmp[i], info, ",")
        freq = info[1]
        chan = info[2]

        # Store Frame Amount
        amount[freq, chan, "Total"]
        amount[freq, chan, "Mgmt"]
        amount[freq, chan, "Ctrl"]
        amount[freq, chan, "Data"]
        
        # Store Frame Size
        size[freq, chan, "Total"]
        size[freq, chan, "Mgmt"]
        size[freq, chan, "Ctrl"]
        size[freq, chan, "Data"]
    }
}

function show() {
    cmd = "sort -n"

    # format the output
    printf "\n\nFreq\tChannel\tEffect\tAmount Size(Total Mgmt Ctrl Data)\n"

    for(tmp in freq2chan) {
        split(tmp, fc, SUBSEP)
        
        freq = fc[1]
        chan = freq2chan[freq]
        eft = out[freq, chan, "effect"]
        
        # Store Frame Amount
        ta = amount[freq, chan, "Total"]
        ma = amount[freq, chan, "Mgmt"]
        ca = amount[freq, chan, "Ctrl"]
        da = amount[freq, chan, "Data"]
        
        # Store Frame Size
        ts = size[freq, chan, "Total"]
        ms = size[freq, chan, "Mgmt"]
        cs = size[freq, chan, "Ctrl"]
        ds = size[freq, chan, "Data"]

        if(chan == "14") continue

        # show output in the ascending order
        printf "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", freq, chan, eft, ta, ts, ma, ms, ca, cs, da, ds | cmd
    }

    close(cmd)
}


BEGIN {
    effect = ARGV[1]
    frame_info = ARGV[2]

    seperate_Effect()
    seperate_FrameInfo()
    show()
}