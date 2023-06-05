#/user/bin/awk -f
#
# This awk script will search for monitor and other interfaces according to the input
# And the output will give which one is monitor interface and the others.
#
# Output foramt is like this:
# interfaces of nonmonitor mode!interfaces of monitor mode !Original Channel
#

function Get_Interface() {
    cmd = "iw dev"
    phy_conf = "none"
    chan = ""
    non_monitor = ""
    monitor = ""

    while(cmd | getline) {
        if(index($0, "phy#") == 1) {
            phy_conf = "phy" substr($1, 5)
        }

        if(phy_conf == phy) {
            if(index($0, "Interface") != 0) {
                itf = $2
            }
            else if(index($0, "type") != 0) {
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
                chan = $2
            }
        }

    }

    close(cmd)

    output = non_monitor "!" monitor "!" chan
    printf "%s", output
}


BEGIN {
    phy = ARGV[1]
    Get_Interface()
}