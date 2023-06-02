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

limit=1
action=""

if [ $# -ne $limit ] 
then
    echo "Option Syntax wrong!"
    show_help
    exit 0
fi

action=$1

if [ "${action}" == "d" ] 
then
    echo "Interface down action mode!"
elif [ "${action}" == "u" ] 
then 
    echo "Interface up action mode!"
else 
    echo "Wrong option!"
    show_help
    exit 0
fi
