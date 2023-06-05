#/usr/bin/awk -f
#
# This awk script will get channel list according to the specific phy
#

function Get_Channel() {
    chan=""
    if(phy == "phy0")
        cmd = "cat channel2G.txt"
    else
        cmd = "cat channel5G.txt"

    while(cmd | getline) {
        if($2 == 14) continue
        chan=chan $2" "
    }
    close(cmd)
    printf "%s", chan
}

BEGIN {
    phy = ARGV[1]
    Get_Channel()
}
