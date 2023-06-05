#/usr/bin/awk -f
#
# This awk script will get channel list according to the specific phy
#

function Get_Channel() {
    chan=""

    # Decide which channel list to read according to the input
    if(phy == "phy0")
        cmd = "cat channel2G.txt"
    else
        cmd = "cat channel5G.txt"

    # Read the channel list and store it
    while(cmd | getline) {
        if($2 == 14 && phy == "phy0") continue
        if($2 > 165 && phy == "phy1") continue
        chan=chan $2" "
    }
    close(cmd)
    printf "%s", chan
}

BEGIN {
    phy = ARGV[1]
    Get_Channel()
}
