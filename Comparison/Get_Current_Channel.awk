#!/usr/bin/awk -f
#
# Extract Current working channel
#

function Get_channel() {

    cmd = "iw dev"
    target = ""
    chan = 0

    # Get target interface addr so that we can scan with grep
    while(cmd | getline) {

        pos = index($1, "#")
        if(pos != 0) {
            split($1, s, "#")
            tmp=s[1] s[2]
            if(phy == tmp) {
                target = "Y"
                continue
            }
        }

        # speed up
        if(target == "")
            continue

        if($1 == "channel") {
            chan = $2
            break
        }
    }
    close(cmd)
    print chan
}

BEGIN {
    phy = ARGV[1]

    Get_channel()
}