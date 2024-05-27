#!/bin/ash
#
# This script is part three of the whole CS process
# And will compare current working channel with other channels
#

# Get Input
data=$1
chan=$2

# Split and Comapre
result=`awk -f Comparison.awk "${data}" $chan`

echo ${result}
