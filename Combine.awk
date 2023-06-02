#!/usr/bin/awk -f
#
# Combine output data from both scanning file
#

function seperate_Effect() {
    n = split(effect, ce, "!")

    for( i = 1; i < n; i++) {
        split(ce[i], s, ",")

        freq2chan[s[1]] = s[2]
        out[s[1], s[2], "effect"] = s[3]
    }

}

function seperate_Amount() {
    n = split(amount, fa, "!")

    for( i = 1; i < n; i++) {
        split(fa[i], s, ",")

        out[s[1], s[2], "amount"] = s[3]
    }
}

function show() {
    cmd = "sort -n"
    printf "\n\nFreq\tChannel\tEffect\tAmount\n"

    for(tmp in freq2chan) {
        split(tmp, fc, SUBSEP)
        
        freq = fc[1]
        chan = freq2chan[freq]
        eft = out[freq, chan, "effect"]
        amt = out[freq, chan, "amount"]
        if(chan == "14") continue

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