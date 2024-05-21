#!/bin/ash
#
# Get Target Channel and joule from input
# And calculate if the diff of joule exceed threshhold
# If so then switch channel
#

# Get Input
data=$1
chan=$2

# Split and Decide
switch=`awk -f Decision.awk "${data}" $chan`

# Output decision result
echo ${switch}


