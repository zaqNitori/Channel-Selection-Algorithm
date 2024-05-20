#!/usr/bin/awk -f
#
# Split input first and then do comapred to search for better channel
#

# Define which area belongs to H M and L, according to RSSI_Throughput experiment result
function definition() {
    RSSI_HIGH_EDGE = -45
    RSSI_MEDIUM_EDGE = -55
}

function extract_data() {
    # Split the original String to get each channel
    n = split(scan_result, tmp, "!")

    # for loop the array
    for(i = 1; i < n; i++) {
        # split each channel string to get effect data
        split(tmp[i], s, ",")

        # create freq and channel mapping
        #freq2chan[s[1]] = s[2]
        channels[s[2]] = s[2]

        # store values and can use chan to get them
        data[s[2], "ta"] = s[3]
        data[s[2], "ts"] = s[4]
        data[s[2], "usage"] = s[5]
        data[s[2], "ugsig"] = s[6]
        data[s[2], "joule"] = s[7]
        data[s[2], "devs"] = s[8]
    }
}

# Firstly, we will use Interfere RSSI to determine which channel is better
# If both chans have similar Interfere RSSI, then we will use Eff_Sig to further compare
# We only comapred with RSSI, no further calculation in this step
function RSSI_Compare() {

    # Get value for current channel
    cur_ugsig = data[cur_chan, "ugsig"]
    cur_usage = data[cu_chan, "usage"]

    for(chan in channels) {

        if(chan == cur_chan)
            continue

        ugsig = data[chan, "ugsig"]


        # Other channel's interfere RSSI is higher or is H
        # if(ugsig >= RSSI_HIGH_EDGE || cur_ugsig < ugsig)
        if(cur_ugsig < ugsig)
            continue
        else if(cur_ugsig > ugsig) {
            # Other channel's interfere RSSI is lower
            candidate[chan] = chan
            continue
        }

    } # End for channel

}

# Secondly, we will try to use other parameters to decide which channel in candidate is the best
# Involve further calculation
function second_compare() {

    cur_joule = data[cur_chan, "joule"]
    cur_usage = data[cur_chan, "usage"]

    for(chan in candidate) {

        joule = data[chan, "joule"]

        # Other channel's receive joule is greater than current channel,
        # which means that channel's usage is also greater than current channel.
        if(joule >= cur_joule)
            continue



    } # End for candidate

}

BEGIN {
    scan_result   = ARGV[1]
    cur_chan = ARGV[2]

    definition()
    extract_data()
    RSSI_Compare()
}
