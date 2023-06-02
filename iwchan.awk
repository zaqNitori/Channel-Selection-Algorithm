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
			dev = gensub(/^\s*\w*\s(\w+)$/, "\\1", 1, $0)
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
	cmd = "iw dev "dev_conf" scan"
	while(cmd | getline) {
		if($0 ~ /^\s*BSS\s/)
			bssid = gensub(/^\s*\w*\s*([:0-9a-f]+).*$/, "\\1", 1, $0)
		else if($0 ~ /^\s*freq:/) {
			freq = gensub(/^\s*\w*:\s*([0-9]+).*$/, "\\1", 1, $0)
			iwscan[bssid, "freq"] = freq
		}
		else if($0 ~ /^\s*signal:/) {
			signal = gensub(/^\s*\w*:\s*([-.0-9]+).*$/, "\\1", 1, $0) + 0
			iwscan[bssid, "signal"] = signal
			if(signal < - 100) quality = 0
			else if(signal < - 50) quality = 2 * (signal + 100)
			else quality = 100
			iwscan[bssid, "quality"] = quality
		}
		else if($0 ~ /^\s*SSID:/) {
			ssid = gensub(/^\s*\w*:\s*(.*)$/, "\\1", 1, $0)
			iwscan[bssid, "ssid"] = ssid
		}
	}
	close(cmd)
}
 
function get_iwload() {
	for(iwphy_subs in iwphy) {
		split(iwphy_subs, iwphy_sub, SUBSEP)
		if(iwphy_sub[3] != "chan") continue
		phy = iwphy_sub[1]
		freq = iwphy_sub[2]
		band = iwphy[phy, freq, "band"]
		load = iwphy[phy, freq, "load"]
		if(band != band_conf) continue
		for(iwscan_subs in iwscan) {
			split(iwscan_subs, iwscan_sub, SUBSEP)
			if(iwscan_sub[2] != "freq") continue
			bssid = iwscan_sub[1]
			freq_bssid = iwscan[bssid, "freq"]
			signal = iwscan[bssid, "signal"]
			freq_diff = freq - freq_bssid
			if(freq_diff < 0) freq_diff = - freq_diff
			if(freq_diff < 5) signal_factor = 100
			else if(freq_diff < 10) signal_factor = 95
			else if(freq_diff < 15) signal_factor = 85
			else if(freq_diff < 20) signal_factor = 15
			else if(freq_diff < 25) signal_factor = 5
			else signal_factor = 0
			if(signal < - 100) load += 0
			else load += (signal + 100) * signal_factor
			iwphy[phy, freq, "load"] = load
		}
	}
}
 
function get_iwstatus() {
	for(iwphy_subs in iwphy) {
		split(iwphy_subs, iwphy_sub, SUBSEP)
		if(iwphy_sub[3] != "chan") continue
		phy = iwphy_sub[1]
		freq = iwphy_sub[2]
		band = iwphy[phy, freq, "band"]
		load = iwphy[phy, freq, "load"]
		if(band != band_conf) continue
		if(load_optim != "" && load_optim < load) continue
		freq_optim = freq
		load_optim = load
	}
	status_conf = "-"
	iwphy[phy_conf, freq_conf, "status"] = status_conf
	status_optim = iwphy[phy_conf, freq_optim, "status"] "+"
	iwphy[phy_conf, freq_optim, "status"] = status_optim
}
 
function get_iwchan() {
	freq_diff = freq_conf - freq_optim
	load_conf = iwphy[phy_conf, freq_conf, "load"]
	load_optim = iwphy[phy_conf, freq_optim, "load"]
	load_diff = load_conf - load_optim
	if(freq_diff < 0) freq_diff = - freq_diff
	if(freq_diff < freq_thr || load_diff < load_thr) return
	chan_optim = iwphy[phy_conf, freq_optim, "chan"]
	printf "%d\n", chan_optim
}
 
function print_iwinfo() {
	printf "Phy:\t%s\nDev:\t%s\nBand:\t%s\nFreqTh:\t%d\nLoadTh:\t%d\n",
		phy_conf, dev_conf, band_conf, freq_thr, load_thr
}
 
function print_iwscan() {
	cmd = "sort -n"
	printf "\nFreq\tChannel\tSignal\tQuality\tBSSID\t\t\tSSID\n"
	for(iwscan_subs in iwscan) {
		split(iwscan_subs, iwscan_sub, SUBSEP)
		if(iwscan_sub[2] != "ssid") continue
		bssid = iwscan_sub[1]
		freq = iwscan[bssid, "freq"]
		chan = iwphy[phy_conf, freq, "chan"]
		signal = iwscan[bssid, "signal"]
		quality = iwscan[bssid, "quality"]
		ssid = iwscan[bssid, "ssid"]
		printf "%d\t%d\t%d\t%d\t%s\t%s\n", freq, chan, signal, quality,
			bssid, ssid | cmd
	}
	close(cmd)
}
 
function print_iwlist() {
	cmd = "sort -n"
	printf "\nFreq\tChannel\tLoad\tStatus\n"
	for(iwphy_subs in iwphy) {
		split(iwphy_subs, iwphy_sub, SUBSEP)
		if(iwphy_sub[3] != "chan") continue
		phy = iwphy_sub[1]
		freq = iwphy_sub[2]
		band = iwphy[phy, freq, "band"]
		chan = iwphy[phy, freq, "chan"]
		load = iwphy[phy, freq, "load"]
		status = iwphy[phy, freq, "status"]
		if(band != band_conf) continue
		printf "%d\t%d\t%d\t%s\n", freq, chan, load, status | cmd
	}
	close(cmd)
}

function my_output() {

	for(iwphy_subs in iwphy) {
		split(iwphy_subs, iwphy_sub, SUBSEP)
		if(iwphy_sub[3] != "chan") continue
		phy = iwphy_sub[1]
		freq = iwphy_sub[2]
		band = iwphy[phy, freq, "band"]
		chan = iwphy[phy, freq, "chan"]
		load = iwphy[phy, freq, "load"]
		if(band != band_conf) continue
		printf "%d,%d,%d!", freq,chan,load
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
		get_iwload()
		get_iwstatus()
		if(subcmd == "get") get_iwchan()
		else if(subcmd == "show") {
			#print_iwinfo()
			#print_iwscan()
			#print_iwlist()
			my_output()
		}
	}
}