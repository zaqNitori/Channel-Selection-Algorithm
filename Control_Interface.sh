#!/bin/ash
#
# This script will up / down the interface
# and will switch interface's channel to specific 
#

function show_help() {
    echo "-d for interface down, which use before channel_hopping so that iw command can successfully working."
    echo "-u for interface up, which use after channel_hopping to set interface config back to origin."
    echo ""
}

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

IsValid "$1"

function Set_Interface_Up() {
    
    for itf in $2
    do
        iw dev ${itf} set channel $3
        break
    done

    for itf in $1
    do
        ifconfig ${itf} up
    done

    sleep 1
    wifi
}

function Set_Interface_Down() {

    for itf in $1
    do
        ifconfig ${itf} down
    done

    for itf in $2
    do
        iw dev ${itf} set channel 14
        break
    done
}

non_monitor=$2
monitor=$3
chan=$4

if [ $1 == "u" ]
then
Set_Interface_Up "${non_monitor}" "${monitor}" "${chan}"
else
Set_Interface_Down "${non_monitor}" "${monitor}" "${chan}"
fi

