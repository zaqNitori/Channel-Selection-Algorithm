#!/usr/bin/awk -f
#
# Split input first and then do comapred to search for better channel
#

function extract_data() {
    # Split the original String to get each channel
    n = split(_input, tmp, "!")

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
function first_compare() {

    # Get value for current channel
    cur_ugsig = data[cur_chan, "ugsig"]
    
    # TODO: Change L with specific RSSI value (dBm)
    # cur_ugsig < ??
    # Current channel is clean, so we don't need to do CS.
    if(cur_ugsig == "L") {
        print 0
        return
    }


    for(chan in channels) {

        if(chan == cur_chan)
            continue

        ugsig = data[chan, "ugsig"]

        # TODO: Change H with specific RSSI value (dBm)
        # ugsig > ??
        # Other channel's interfere RSSI is higher
        if(ugsig == "H" || cur_ugsig < ugsig)
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
    _input   = ARGV[1]
    cur_chan = ARGV[2]

    extract_data()
    first_compare()
}
