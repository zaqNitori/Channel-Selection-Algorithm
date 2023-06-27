#/user/bin/awk -f
#
# This awk script will search for monitor and other interfaces according to the input
# And the output will give which one is monitor interface and the others.
#
# Output foramt is like this:
# interfaces of nonmonitor mode!interfaces of monitor mode !Original Channel
#

function Get_Interface() {
    #Initial variables
    cmd = "iw dev"
    phy_conf = "none"
    chan = ""
    non_monitor = ""
    monitor = ""

    # Call iw dev to get interfaces
    while(cmd | getline) {
        # Store each phy
        if(index($0, "phy#") == 1) {
            phy_conf = "phy" substr($1, 5)
        }

        if(phy_conf == phy) {
            
            if(index($0, "Interface") != 0) {
                # Memorize the interface
                itf = $2
            }
            else if(index($0, "type") != 0) {
                # Check which mode is this interface in
                if($2 == "monitor") {
                    if(monitor == "")
                        monitor = itf
                    else
                        monitor = monitor " " itf
                }
                else {
                    if(non_monitor == "")
                        non_monitor = itf
                    else
                        non_monitor = non_monitor " " itf
                }
            }
            else if(index($0, "channel") != 0) {
                # Check the original channel used in this phy
                chan = $2
            }
        }

    }

    close(cmd)

    # Combine the data to return
    output = non_monitor "!" monitor "!" chan

    # waiting channel
    if(phy == "phy0")
        waiting = "14"
    else
        waiting = "100"
    
    output = output"!"waiting

    printf "%s", output
}


BEGIN {
    phy = ARGV[1]
    Get_Interface()
}