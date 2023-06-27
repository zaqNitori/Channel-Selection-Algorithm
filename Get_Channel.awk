#/usr/bin/awk -f
#
# This awk script will get channel list according to the specific phy
#

function extract(str) {
    return substr(str, 2, length(str) - 2)
}

function Get_Channel() {

    chan=""
    limit=5000
    
    # Search available channel by using iw phy command
    cmd="iw phy | grep -E \"MHz.*dBm\""

    # Read the channel list and store it
    while(cmd | getline) {

        # Determine the current phy interface and filtout the unmatched
        if($2 > limit && phy == "phy0") continue
        if($2 < limit && phy == "phy1") continue

        if(chan == "")
            chan = extract($4)
        else
            chan=chan" "extract($4)

    }
    close(cmd)
    printf "%s", chan
}

BEGIN {
    phy = ARGV[1]
    Get_Channel()
}
