#!/usr/bin/awk -f
#
# Use tcpdump to scan frames
#

function extract(str) {
    return substr(str, 2, length(str) - 2)
}

function Initial() {
    
    # Search available channel by using iw phy command
    cmd="iw phy "phy" channels | grep \"*\" | grep -v \"disabled\""

    # Initial the array while reading the channel list
    while(cmd | getline) {

        freq                       = $2
        chan                       = extract($4)

        if(phy == "phy0" && (chan+0) > 11)
            continue
        if(phy == "phy1" && (chan+0) > 48)
            continue

        freq2chan[phy, freq]       = chan
        chan2freq[phy, chan]       = freq
        amount[phy, chan, "Total"] = 0
        size[phy, chan, "Total"]   = 0
        duration[phy, chan]        = 0
        joule[phy, chan]           = 0.0

        if(debug) {
            amount[phy, chan, "Mgmt"]  = 0
            amount[phy, chan, "Ctrl"]  = 0
            amount[phy, chan, "Data"]  = 0
            size[phy, chan, "Mgmt"]    = 0
            size[phy, chan, "Ctrl"]    = 0
            size[phy, chan, "Data"]    = 0
        }
    }
    close(cmd)

    Extract_MAC_Key[0] = "SA:"
    Extract_MAC_Key[1] = "TA:"
}

# Initialize Array, and add broadcast MAC Addr.
function Initial_MAC_Record() {
    mac_record["ff:ff:ff:ff:ff:ff"] = 1
}

# Count how much devices on each channel
# Should Minus broadcast mac addr
function Calculate_MAC_Record() {
    dev = -1
    for(i in mac_record) {
        dev += 1
    }
    numDev[chan] = dev
    delete mac_record
}

function Scan() {
    
    pre_freq = 0
    cmd = "tcpdump -ne -y ieee802_11_radio -i "interface" -v -t -s0 -e | grep \"!\""

    # Reading Result of Tcpdump
    while(cmd | getline) {
        pos = index($0, "MHz")
        if(pos == 0) continue

        # Extract the freq
        {
            freq = substr($0, pos-5, 4)
            if(pre_freq != freq) {
                if(pre_freq != 0)
                    Calculate_MAC_Record()

                Initial_MAC_Record()
                pre_freq = freq
            }
            chan = freq2chan[phy, freq]
            if(chan == 14) continue
        }

        # Record MAC Addr to calculate how much diff devices on each channel
        # BSSID, SA, DA, RA, TA
        # Only consider SA and TA since they are the actual sender
        {
            for(i in Extract_MAC_Key) {

                # Get Key
                key = Extract_MAC_Key[i]

                # Get Position
                pos = index($0, key)
                if(pos == 0) continue

                # Extract MAC Addr
                mac = substr($0, pos+3, 17)

                # Record
                if(mac_record[mac] == 0) {
                    mac_record[mac] = 1
                }
            }
        }


        # Extract type and size
        # awk starts from index 1
        {
            split($0, tmp, "!")
            split(tmp[2], good)
            type = good[1]
            size = good[2]
            dura = good[3]
        }

        # Counting the frame amounts
        {
            amount[phy, chan, "Total"]++
            if(debug) {
                amount[phy, chan, type]++
            }
        }

        # Collecting the frame sizes
        {
            size[phy, chan, "Total"] += size + 0
            if(debug) {
                size[phy, chan, type] += size + 0
            }
        }

        # Collecting the duration
        {
            duration[phy, chan] += dura
        }

        # Calculate Energy Consume Energy = Power(mWatt) x Time(us)
        {
            pos = index($0, "dBm")
            if(pos == 0) continue
            
            tmp = substr($0, pos - 4, 4)
            cnt = split(tmp, sig, " ")
            if(cnt == 1)
                dbm = (sig[1] + 0)
            else
                dbm = (sig[2] + 0)
            
            watt = 10 ^ (dbm / 10.0)
            joule[phy, chan] += dura * watt
        }
    }
    close(cmd)

    # Last channel will not be calculate, so should call again
    Calculate_MAC_Record()
}

function Show() {

    for(subs in chan2freq) {
        split(subs, tmp, SUBSEP)
        
        phy = tmp[1]
        chan = tmp[2]
        freq = chan2freq[phy, chan]

        # Get Frame Amount
        ta = amount[phy, chan, "Total"]
        if(debug) {
            ma = amount[phy, chan, "Mgmt"]
            ca = amount[phy, chan, "Ctrl"]
            da = amount[phy, chan, "Data"]
        }
        
        # Get Frame Size
        ts = size[phy, chan, "Total"]
        if(debug) {
            ms = size[phy, chan, "Mgmt"]
            cs = size[phy, chan, "Ctrl"]
            ds = size[phy, chan, "Data"]
        }

        # Get duration and calculate usage
        # Plus 0.5 to do rounding
        dura = duration[phy, chan]
        usage = (dura * 100) / (1000000 * interval) + 0.5

        # Get Energy collect from duration
        # And calculate avg_dbm during duration time
        j = joule[phy, chan]
        if(dura > 0)
            ug_sig = 10 * (log(j / dura) / log(10))
        else
            ug_sig = -100

        devs = numDev[chan]

        if(debug) {
            # print all data
            printf "%d,%d,%d,%d,%d,%d,%.3f,%d,%d,%d,%d,%d,%d,%d!", freq, chan, ta, ts, usage, ug_sig, j, devs, ma, ms, ca, cs, da, ds

        }
        else {
            # print only needed data
            printf "%d,%d,%d,%d,%d,%d,%.3f,%d\n", freq, chan, ta, ts, usage, ug_sig, j, devs
        }
    }

}

BEGIN {
    phy       = ARGV[1]
    interface = ARGV[2]
    interval  = ARGV[3]
    debug     = ARGV[4]

    Initial()
    Scan()
    Show()
}