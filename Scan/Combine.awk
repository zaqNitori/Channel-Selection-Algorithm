#!/usr/bin/awk -f
#
# Combine output data from both scanning file
#

function seperate_Effect() {
    # Split the original String to get each channel
    n = split(effect, ce, "!")

    # for loop the array
    for( i = 1; i < n; i++) {
        # split each channel string to get effect data
        split(ce[i], s, ",")

        # create freq and channel mapping
        freq2chan[s[1]] = s[2]

        # store dbm and aps values and can use freq + chan to get them
        out[s[1], s[2], "dbm"] = s[3]
        out[s[1], s[2], "aps"] = s[4]
    }

}

function seperate_FrameInfo() {
    # Split the original String to get each channel
    n = split(frame_info, tmp, "!")
    fix_parameter = 8

    # for loop the array
    for( i = 1; i < n; i++) {
        
        # split each channel string to get amount data
        split(tmp[i], info, ",")
        freq = info[1]
        chan = info[2]

        # Store Frame Amount
        amount[freq, chan, "Total"] = info[3]
        if(debug) {
            amount[freq, chan, "Mgmt"] = info[fix_parameter + 1]
            amount[freq, chan, "Ctrl"] = info[fix_parameter + 3]
            amount[freq, chan, "Data"] = info[fix_parameter + 5]
        }
        
        # Store Frame Size
        size[freq, chan, "Total"] = info[4]
        if(debug) {
            size[freq, chan, "Mgmt"] = info[fix_parameter + 2]
            size[freq, chan, "Ctrl"] = info[fix_parameter + 4]
            size[freq, chan, "Data"] = info[fix_parameter + 6]
        }

        # Store Usage
        usage[freq, chan] = info[5]

        # Store Avg dBm during usage
        usage_Signal[freq, chan] = info[6]

        # Store Joule
        joule[freq, chan] = info[7]

        # Store Number of Devs
        numDev[freq, chan] = info[8]
    }
}

function show() {

    for(tmp in freq2chan) {
        split(tmp, fc, SUBSEP)

        freq = fc[1]
        chan = freq2chan[freq]
        sig = out[freq, chan, "dbm"]
        aps = out[freq, chan, "aps"]

        # Store Frame Amount
        ta = amount[freq, chan, "Total"]
        
        # Store Frame Size
        ts = size[freq, chan, "Total"]

        # Store channel usage
        ug = usage[freq, chan] 

        # Store avg dbm during duration time
        ug_sig = usage_Signal[freq, chan]

        # Store joule
        j = joule[freq, chan]

        # Store number of Dev
        devs = numDev[freq, chan]

        # show output in the ascending order
        printf "%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%d!", freq, chan, sig, aps, ta, ts, ug, ug_sig, j, devs
    }

}

function show_debug() {
    cmd = "sort -n"

    # format the output
    printf "Freq\tChannel\tSignal\tAPs\tTotal_A\tTotal_S\tUsage\tU_Sig\tJoule\tDevs\tMgmt_A\tMgmt_S\tCtrl_A\tCtrl_S\tData_A\tData_S\n"

    for(tmp in freq2chan) {
        split(tmp, fc, SUBSEP)
        
        freq = fc[1]
        chan = freq2chan[freq]
        sig = out[freq, chan, "dbm"]
        aps = out[freq, chan, "aps"]
        
        # Store Frame Amount
        ta = amount[freq, chan, "Total"]
        ma = amount[freq, chan, "Mgmt"]
        ca = amount[freq, chan, "Ctrl"]
        da = amount[freq, chan, "Data"]
        
        # Store Frame Size
        ts = size[freq, chan, "Total"]
        ms = size[freq, chan, "Mgmt"]
        cs = size[freq, chan, "Ctrl"]
        ds = size[freq, chan, "Data"]

        # Store channel usage
        ug = usage[freq, chan] 

        # Store avg dbm during duration time
        ug_sig = usage_Signal[freq, chan]

        # Store joule
        j = joule[freq, chan]

        # Store number of Dev
        devs = numDev[freq, chan]

        # show output in the ascending order
        printf "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", freq, chan, sig, aps, ta, ts, ug, ug_sig, j, devs, ma, ms, ca, cs, da, ds | cmd
    }

    close(cmd)
}


BEGIN {
    effect     = ARGV[1]
    frame_info = ARGV[2]
    debug      = ARGV[3]

    seperate_Effect()
    seperate_FrameInfo()

    if(debug) {
        show_debug()
    }
    else {
        show()
    }
}