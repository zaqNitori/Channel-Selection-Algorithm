#!/bin/ash
#
# This script is part three of the whole CS process
# And will compare current working channel with other channels
#

# Get Input
phy=$1
data=$2

chan=`awk -f Get_Current_Channel.awk "${phy}"`

# Split and Comapre
result=`awk -f Comparison.awk "${data}" $chan`

echo ${result}
