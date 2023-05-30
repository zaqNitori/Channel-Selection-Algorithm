#!/bin/ash
#
# This script will call iwchan.awk first to get channel effect
# and then call frame scan process to capture the flow data amount 
# and combine them to show a more precisely channel measurement.
#

# Will Create an empty file and call iwchan.awk to store the output data
touch result & awk -f iwchan.awk show phy0 > result

# Then will call frame scan

