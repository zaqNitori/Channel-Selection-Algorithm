#!/bin/ash
#
# This script will up / down the interface
# and will switch interface's channel to specific 
#

# Show help messages
function show_help() {
    echo "-d for interface down, which use before channel_hopping so that iw command can successfully working."
    echo "-u for interface up, which use after channel_hopping to set interface config back to origin."
    echo ""
}

# Check is input correct or not
function IsValid() {

    if [ $1 == "d" ] 
    then
        echo "Interface down action mode!"
    elif [ $1 == "u" ] 
    then 
        echo "Interface up action mode!"
    else 
        echo "Wrong option!"
        show_help
        exit 0
    fi
}

# Check input first
IsValid "$1"


function Set_Interface_Up() {
    
    # Set channel back to the original channel first, otherwise will be denied
    for itf in $2
    do
        iw dev ${itf} set channel $3
        break
    done

    # And then set interfaces up
    for itf in $1
    do
        ifconfig ${itf} up
    done

    # wait 1s and then reload and restart the service
    sleep 1
    wifi
}

function Set_Interface_Down() {

    # Set interfaces down first, so that we can set channel successfully
    for itf in $1
    do
        ifconfig ${itf} down
    done

    # Set channel to waiting channel before channel hopping
    for itf in $2
    do
        iw dev ${itf} set channel $3
        break
    done
}

# Get input values.
# And these input values more likely be in string arrays
non_monitor=$2
monitor=$3
chan=$4

# decide which action to do depend on the given input
if [ $1 == "u" ]
then
Set_Interface_Up "${non_monitor}" "${monitor}" "${chan}"
else
Set_Interface_Down "${non_monitor}" "${monitor}" "${chan}"
fi

