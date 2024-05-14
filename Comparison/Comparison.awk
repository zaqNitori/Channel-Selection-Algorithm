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

        # store values and can use freq + chan to get them
        data[s[2], "eff_sig"] = s[3]
        data[s[2], "eff_aps"] = s[4]
        data[s[2], "ta"] = s[5]
        data[s[2], "ts"] = s[6]
        data[s[2], "usage"] = s[7]
        data[s[2], "ugsig"] = s[8]
        data[s[2], "joule"] = s[9]
    }
}

# Firstly, we will use Interfere RSSI to determine which channel is better
# If both chans have similar Interfere RSSI, then we will use Eff_Sig to further compare
# We only comapred with RSSI, no further calculation in this step
function RSSI_Compare() {

    # Get value for current channel
    cur_ugsig = data[cur_chan, "ugsig"]

    for(chan in channels) {

        if(chan == cur_chan)
            continue

        ugsig = data[chan, "ugsig"]


        # Other channel's interfere RSSI is higher or is H
        if(ugsig >= RSSI_HIGH_EDGE || cur_ugsig < ugsig)
            continue

        # Other channel's interfere RSSI is lower
        if(cur_ugsig > ugsig) {
            candidate[chan] = chan
            continue
        }

        # MM and LL
        # Should further use Eff APs and Eff Sig to compare
        {
            cur_effsig = data[cur_chan, "eff_sig"]
            cur_aps = data[cur_chan, "eff_aps"]
            eff_sig = data[chan, "eff_sig"]
            eff_aps = data[chan, "eff_aps"]

            if(eff_sig < cur_effsig)
                candidate[chan] = chan
            else if(eff_sig == cur_effsig && eff_aps < cur_aps)
                candidate[chan] = chan
        }
    } # End for channel

}


BEGIN {
    scan_result   = ARGV[1]
    cur_chan = ARGV[2]

    definition()
    extract_data()
    RSSI_Compare()
}
