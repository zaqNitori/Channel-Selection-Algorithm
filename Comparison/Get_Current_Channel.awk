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
        if($2 == target_itf) {
            target = "Y"
            continue
        }
        
        if(target != "" && $1 == "Interface") {
            chan = 0
            break
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
    target_itf = ARGV[1]

    Get_channel()
}