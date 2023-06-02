#!/usr/bin/awk -f
#
# Combine output data from both scanning file
#

function seperate_Effect() {
    n = split(effect, ce, "!")

    for( i = 0; i < n; i++) {
        print ce[i]
    }

}

function seperate_Amount() {
    n = split(amount, fa, "!")

    for( i = 0; i < n; i++) {
        print fa[i]
    }
}



BEGIN {
    effect = ARGV[1]
    amount = ARGV[2]

    seperate_Effect()
    seperate_Amount()

}