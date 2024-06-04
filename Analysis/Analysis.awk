#!/usr/bin/awk -f
#
# Split input first and then do comapred to search for better channel
#

# Define which area belongs to H M and L, according to RSSI_Throughput experiment result
function definition() {
    RSSI_HIGH_EDGE = -45
    RSSI_MEDIUM_EDGE = -55

    USAGE_THRESHOLD = 25

    # ICI Factor
    factor_ICI[1] = 1.13
    factor_ICI[-1] = 1.13
    factor_ICI[2] = 1.15
    factor_ICI[-2] = 1.15
    factor_ICI[3] = 1.23
    factor_ICI[-3] = 1.23

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

# Calculate from overlapping area of adjacent channel
# we give factor of diff channel 2, 3, 4 => 0.75, 0.5, 0.25
# And both Usage and Joule will take ICI into account.
function Calculate_ICI() {

    for(chan in channels) {
        tmp_chan = 0
        tmp_joule = 0
        tmp_usage = 0

        for(i = -3;i <= 3; i++) {
            if(!i)
                continue

            tmp_chan = chan + i
            tmp_joule += factor_ICI[i] * data[tmp_chan, "joule"]
            tmp_usage += factor_ICI[i] * data[tmp_chan, "usage"]
        }

        # Record ICI info in diff arrays, so it won't effect lately calculation
        data_ICI[chan, "usage"] = tmp_usage
        data_ICI[chan, "joule"] = tmp_joule
    }

    for(chan in channels) {
        # Add ICI back, after whole ICI calculation finish
        data[chan, "joule"] += data_ICI[chan, "joule"]
        data[chan, "usage"] += data_ICI[chan, "usage"]
    }

    cur_usage = data[cur_chan, "usage"]
    if(cur_usage < USAGE_THRESHOLD) {
        # Output 1 which means current channel's usage does not exceed usage threshold
        # So will not continue.
        print 1
        exit 0
    }

}

# The rules we use to compare each channel is calculated from truth-table
# Candidate = !Usage + !UgSig!Devs
function Choose_Candidate() {

    # Get value for current channel
    cur_ugsig = data[cur_chan, "ugsig"]
    cur_usage = data[cur_chan, "usage"]
    cur_devs = data[cur_chan, "devs"]

    for(chan in channels) {

        if(chan == cur_chan)
            continue

        ugsig = data[chan, "ugsig"]
        usage = data[chan, "usage"]
        devs = data[chan, "devs"]

        if(usage < cur_usage) {
            # Other channel's Usage is less
            candidate[chan] = chan
        }
        else if(ugsig < cur_ugsig && devs < cur_devs) {
            # Other channel's UgSig and devs both are less
            candidate[chan] = chan
        }

    } # End for channel

}

# Foreach chan in candidate, we compare with their joule 
# and select one chan with the smallest joule value.
function Compare() {

    min_joule = data[cur_chan, "joule"]
    tmp_chan = 0
    

    for(chan in candidate) {
        
        joule = data[chan, "joule"]

        # Compare joule for each channel, and select one with the smallest joule value
        if(joule < min_joule) {
            min_joule = joule
            tmp_chan = chan
        }
    }
}

# Show current chan's info and select chan's info
function Show() {

    if(!tmp_chan) {
        # tmp_chan = 0, means no chan better than current chan
        print tmp_chan
    }
    else {

        cur_joule = data[cur_chan, "joule"]
        tmp_joule = data[tmp_chan, "joule"]

        printf "%d,%.3f!", cur_chan, cur_joule
        printf "%d,%.3f!", tmp_chan, tmp_joule
    }
}

BEGIN {
    scan_result   = ARGV[1]
    cur_chan = ARGV[2]

    definition()
    extract_data()
    Calculate_ICI()
    Choose_Candidate()
    Compare()
    Show()
}
