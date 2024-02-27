#!/usr/bin/awk -f
#
# Use tcpdump to scan without channel hopping
#

#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function extract(str) {
    return substr(str, 2, length(str) - 2)
}

function Scan() {
    
    total_amount = 0
    total_size = 0
    duration = 0
    cmd = "tcpdump -ne -y ieee802_11_radio -i "interface" -e | grep \"!\""

    # Reading Result of Tcpdump
    while(cmd | getline) {

        # Extract type and size
        # awk starts from index 1
        split($0, tmp, "!")
        split(tmp[2], good)
        type = good[1]
        size = good[2]
        dura = good[3]

        # Counting the frame amounts
        total_amount += 1

        # Collecting the frame sizes
        total_size += size + 0

        # Collecting the duration
        duration += dura
    }
    close(cmd)
}

function Show() {

    # Get duration and calculate usage
    # Plus 0.5 to do rounding
    #usage = (duration * 100) / (1000000 * interval) + 0.5
    #usage = (duration) / (10000 * interval) + 0.5

    printf "%d,%d,%d", total_amounts, total_size, duration

}

BEGIN {
    interface = ARGV[1]

    Scan()
    Show()
}
