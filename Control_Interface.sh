#!/bin/ash
#
# This script will up / down the interface
# and will switch interface's channel to specific 
#

logFile="logCS"
echo "---------Control_Interface.sh---------" >> "${logFile}"

# Show help messages
function show_help() {
    echo "-d for interface down, which use before channel_hopping so that iw command can successfully working."
    echo "-u for interface up, which use after channel_hopping to set interface config back to origin."
    echo ""
}

# Check is input correct or not
function IsValid() {

    echo "IsValid()" >> "${logFile}"
    down="Interface down action mode!"
    up="Interface up action mode!"

    if [ $1 == "d" ]; then
        if [ $2 -eq 1 ]; then
            echo "${down}" | tee -a "${logFile}"
        else
            echo "${down}" >> "${logFile}"
        fi
    elif [ $1 == "u" ]; then
        if [ $2 -eq 1 ]; then
            echo "${up}" | tee -a "${logFile}"
        else
            echo "${up}" >> "${logFile}"
        fi
    else 
        echo "Wrong option!"
        show_help
        exit 0
    fi
}

function Set_Interface_Up() {
    
    echo "Set_Interface_Up()" >> "${logFile}"

    # Set channel back to the original channel first, otherwise will be denied
    for itf in $2
    do
        iw dev ${itf} set channel $3

        cmd="iw dev ${itf} set channel $3"
        if [ $4 -eq 1 ]; then
            echo "${cmd}" | tee -a "${logFile}"
        else
            echo "${cmd}" >> "${logFile}"
        fi
        break
    done

    # And then set interfaces up
    for itf in $1
    do
        ifconfig ${itf} up

        cmd="ifconfig ${itf} up"
        if [ $4 -eq 1 ]; then
            echo "${cmd}" | tee -a "${logFile}"
        else
            echo "${cmd}" >> "${logFile}"
        fi
    done

    # wait 1s and then reload and restart the service
    sleep 1
    wifi >> "${logFile}"
}

function Set_Interface_Down() {

    echo "Set_Interface_Down()" >> "${logFile}"

    # Set interfaces down first, so that we can set channel successfully
    for itf in $1
    do
        ifconfig ${itf} down
        
        cmd="ifconfig ${itf} down"
        if [ $4 -eq 1 ]; then
            echo "${cmd}" | tee -a "${logFile}"
        else
            echo "${cmd}" >> "${logFile}"
        fi
    done

    # Set channel to waiting channel before channel hopping
    for itf in $2
    do
        iw dev ${itf} set channel $3
        
        cmd="iw dev ${itf} set channel $3"
        if [ $4 -eq 1 ]; then
            echo "${cmd}" | tee -a "${logFile}"
        else
            echo "${cmd}" >> "${logFile}"
        fi
        break
    done
}

# Get input values.
# And these input values more likely be in string arrays
act=$1
non_monitor=$2
monitor=$3
chan=$4
debug=$5

# Check input first
IsValid "${act}" $debug

# decide which action to do depend on the given input
if [ $1 == "u" ]
then
Set_Interface_Up "${non_monitor}" "${monitor}" "${chan}" $debug
else
Set_Interface_Down "${non_monitor}" "${monitor}" "${chan}" $debug
fi

