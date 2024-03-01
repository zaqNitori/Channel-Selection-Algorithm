#!/usr/bin/awk -f
 
# Coded by: Vladislav Grigoryev <vg[dot]aetera[at]gmail[dot]com>
# License: GNU General Public License (GPL) version 3+
# Description: Select wireless channel automatically
 
function get_iwphy() {
	cmd = "iw phy"
	while(cmd | getline) {
		if($0 ~ /^\s*Wiphy\s/)
			phy = gensub(/^\s*\w*\s(\w+)$/, "\\1", 1, $0)
		else if($0 ~ /^\s*Band\s/)
			band = gensub(/^\s*\w*\s([0-9]+):$/, "\\1", 1, $0)
		else if($0 ~ /^\s*\*\s*[0-9]+\s*MHz.*dBm/) {
			freq = gensub(/^.*\s([0-9]+)\s*MHz.*$/, "\\1", 1, $0)
			chan = gensub(/^.*\[([0-9]+)\].*$/, "\\1", 1, $0)
			iwphy[phy, freq, "band"] = band
			iwphy[phy, freq, "chan"] = chan
		}
	}
	close(cmd)
}
 
function get_iwdev() {
	cmd = "iw dev"
	while(cmd | getline) {
		if($0 ~ /^\s*phy\x23/)
			phy = gensub(/^\s*(\w+)\x23([0-9]+)$/, "\\1\\2", 1, $0)
		else if($0 ~ /^\s*Interface\s/)
			dev = $2
		else if($0 ~ /^\s*channel\s/) {
			freq = gensub(/^.*\(([0-9]+)\s*MHz\).*$/, "\\1", 1, $0)
			iwdev[phy, dev, "freq"] = freq
		}
	}
	close(cmd)
}
 
function get_iwconf() {
	for(iwdev_subs in iwdev) {
		split(iwdev_subs, iwdev_sub, SUBSEP)
		if(iwdev_sub[3] != "freq") continue
		phy = iwdev_sub[1]
		dev = iwdev_sub[2]
		if(phy_conf == "") phy_conf = phy
		if(phy_conf != phy) continue
		dev_conf = dev
		freq_conf = iwdev[phy_conf, dev_conf, "freq"]
		band_conf = iwphy[phy_conf, freq_conf, "band"]
	}
}
 
function get_iwscan() {
	cmd = "iw dev "dev_conf" scan | grep -E 'freq:|signal:'"

	cnt = 0
	while(cmd | getline) {
		
		if((cnt % 2) == 0) {
			freq = $2
			aps = iwphy[phy_conf, freq, "aps"]

			# Store the number of APs
			aps += 1
			iwphy[phy_conf, freq, "aps"] = aps
		}
		else {
			signal = $2
			# load represent the expect interference from surrounding APs
			watt = iwphy[phy_conf, freq, "watt"]
			watt += 10 ^ (signal / 10.0)
			iwphy[phy_conf, freq, "watt"] = watt
			# printf "freq: %d, ", freq
			# printf "signal: %d\n", signal
		}

		# can calculate avg signal by using following formula
		# (load / 100) / (load % 100) => total signal / total APs
		cnt += 1
	}
	close(cmd)
}

function calculate_avg_dbm() {
	for(iwphy_subs in iwphy) {
		split(iwphy_subs, iwphy_sub, SUBSEP)
		if(iwphy_sub[3] != "watt") continue
		phy = iwphy_sub[1]
		freq = iwphy_sub[2]
		aps = iwphy[phy, freq, "aps"]
		watt = iwphy[phy, freq, "watt"]
		if(aps == 0) continue
		avg_watt = watt * 1.0 / aps
		avg_dbm = 10 * (log(avg_watt) / log(10))
		iwphy[phy, freq, "avg_dbm"] = avg_dbm
	}
}

function my_output() {

	for(iwphy_subs in iwphy) {
		split(iwphy_subs, iwphy_sub, SUBSEP)
		if(iwphy_sub[3] != "chan") continue
		phy = iwphy_sub[1]
		freq = iwphy_sub[2]
		band = iwphy[phy, freq, "band"]
		chan = iwphy[phy, freq, "chan"]
		avg_dbm = iwphy[phy, freq, "avg_dbm"]
		aps = iwphy[phy, freq, "aps"]
		if(band != band_conf) continue
		printf "%d,%d,%d,%d!", freq, chan, avg_dbm, aps
	}
}

BEGIN {
	subcmd = ARGV[1]
	phy_conf = ARGV[2]
	freq_thr = ARGV[3]
	load_thr = ARGV[4]
	if(subcmd == "") subcmd = "help"
	if(freq_thr == "") freq_thr = 15
	if(load_thr == "") load_thr = 1000
	if(subcmd == "help")
		printf "awk -f iwchan.awk [show|get|help] [phy] [freq_thr] [load_thr]\n"
	else if(subcmd == "get" || subcmd == "show") {
		get_iwphy()
		get_iwdev()
		get_iwconf()
		get_iwscan()
		calculate_avg_dbm()
		if(subcmd == "show") {
			my_output()
		}
	}
}