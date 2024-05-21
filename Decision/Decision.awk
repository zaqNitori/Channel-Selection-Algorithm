#!/usr/bin/awk -f
#
# Calculate the diff of joule for current channel and target channel
#

# define the threshold of channel switch
function definition() {
    # In percentage %
    IMPROVE_THRESHOLD = 50
}

function extract_data() {
    # Split the original String to get each channel
    n = split(input_data, tmp, "!")

    # for loop the array
    for(i = 1; i < n; i++) {
        # extract channel and joule from each string
        split(tmp[i], s, ",")

        # record joule value
        if(s[1] == cur_chan) {
            cur_joule = s[2]
        }
        else {
            target_joule = s[2]
            target_chan = s[1]
        }
    }

}

# Calculate if the target channel is good enough
function Calculate() {

    improve = (cur_joule - target_joule) / cur_joule * 100
    
    if(improve >= IMPROVE_THRESHOLD) {
        # Set switch as target channel
        switch = target_chan
    }
    else {
        # Set switch as current channel
        switch = cur_chan
    }
    
}

# Show our decision about should we switch channel
function Show() {

    print switch
}

BEGIN {
    input_data = ARGV[1]
    cur_chan   = ARGV[2]

    definition()
    extract_data()
    Calculate()
    Show()
}




