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

        tmp = extract($4)
        # (tmp+0) => cast string to int
        if(phy == "phy0" && (tmp+0) > 11) {
            continue
        }
        if(phy == "phy1" && (tmp+0) > 48){
            continue
        }

        if(chan == "")
            chan = tmp
        else
            chan=chan" "tmp

    }
    close(cmd)

    if(phy == "phy0")
        chan=chan" 14"
    else
        chan=chan" 100"
        
    printf "%s", chan
}

BEGIN {
    phy = ARGV[1]
    Get_Channel()
}
