#/usr/bin/awk -f
#
# This awk script will get channel list according to the specific phy
#

function extract(str) {
    return substr(str, 2, length(str) - 2)
}

function Get_Channel() {

    chan=""
    
    # Search available channel by using iw phy command
    cmd="iw phy "phy" channels | grep \"*\" | grep -v \"disabled\""

    # Read the channel list and store it
    while(cmd | getline) {

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
