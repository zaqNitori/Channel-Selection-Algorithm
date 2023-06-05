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

function seperate_Amount() {
    # Split the original String to get each channel
    n = split(amount, fa, "!")

    # for loop the array
    for( i = 1; i < n; i++) {
        # split each channel string to get amount data
        split(fa[i], s, ",")

        # store effect values and can use freq + chan to get them
        out[s[1], s[2], "amount"] = s[3]
    }
}

function show() {
    cmd = "sort -n"

    # format the output
    printf "\n\nFreq\tChannel\tEffect\tAmount\n"

    for(tmp in freq2chan) {
        split(tmp, fc, SUBSEP)
        
        freq = fc[1]
        chan = freq2chan[freq]
        eft = out[freq, chan, "effect"]
        amt = out[freq, chan, "amount"]
        if(chan == "14") continue

        # show output in the ascending order
        printf "%d\t%d\t%d\t%d\n", freq, chan, eft, amt | cmd
    }

    close(cmd)
}


BEGIN {
    effect = ARGV[1]
    amount = ARGV[2]

    seperate_Effect()
    seperate_Amount()
    show()
}