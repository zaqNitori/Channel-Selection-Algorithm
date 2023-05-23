#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function initial() {
    cmd = "cat channel.txt"
    while(cmd | getline) {
        freq = $1
        chan = $2
        channel[freq] = chan
    }
}

function scan() {
    loop = 1
    cmd = "tcpdump -ne -y ieee802_11_radio -i wlan0-1 -v -t -s0 -e"
    while(cmd | getline) {
        pos = index($0, "MHz")
        chan = substr($0, pos-5, 4);
        frame[]
    }
}

function END_scan() {
    printf "\n!!! Scan End!!!\n\n"
}

BEGIN {
    initial()
    #scan()
    END_scan()
}