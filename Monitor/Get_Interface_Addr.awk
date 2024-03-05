#!/usr/bin/awk -f
#
# Extract Interface Addr
#

function Get_itf_addr() {

    cmd = "iw dev"
    target = ""

    # Get target interface addr so that we can scan with grep
    while(cmd | getline) {
        if($2 == target_itf)
            target = "Y"
        
        # speed up
        if(target == "")
            continue

        if($1 == "addr") {
            target = $2
            break
        }
    }
    close(cmd)
    print target
}

BEGIN {
    target_itf = ARGV[1]

    Get_itf_addr()
}