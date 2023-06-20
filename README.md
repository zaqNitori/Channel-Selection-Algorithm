# Channel-Selection-Algorithm under OpenWrt
This algorithm helps us to calculate the channel interference and data flow between each channel and combine them to search for a better channel.

## Project Description
### What does this algorithm does?
* First will use "iw scan" command to collect the beacons and figure out the nearby sources and use these beacons with their signal value in dBm to 
calculate the channel interference. 
* Secondly, will use tcpdump with channel hopping to monitor the link layer frame to measure the data flow between these sources.

### Challenges Faced
* The origin output for ieee802_11_radio in tcpdump does not show the type of framem also it does not show the payload length. So the data flow we get is not accuracy so far.

## How to Install and Run the Project
* The OpenWrt Version currently in use is OpenWrt 22.03.2

1. For your router devices, please goto following url to check if OpenWrt supports your devices and flash them into OpenWrt System.<br />
[Table Of Hardware](https://openwrt.org/toh/start)
2. Use putty or ssh or luci to login to your OpenWrt devices and then install tcpdump package. <br />
[Log into your router running OpenWrt](https://openwrt.org/docs/guide-quick-start/walkthrough_login)<br />
[Opkg package manager](https://openwrt.org/docs/guide-user/additional-software/opkg)
3. Clone this repo and move all files into the router.
```
scp * or <filename> <user-name>@<target-ip>:~/<path-to-your-folder>
```
4. Use following command to check does there exist at least one monitor mode and one non-monitor mode in the target phy.
```
iw dev
```
* Use following command to add a interface
```
iw phy <phyname> interface add <name> type <type> 
ex: iw phy phy0 interface add moni0 type monitor
```
* Make Sure to enable the interface
```
ifconfig <name> up
ex: ifconfig moni0 up
```
5. Run the following command to change the authentication of the script
```
chmod u+x *.sh
```
6. Can use following command to run the algorithm
```
./Channel_Selection.sh -p <phyname> [-s] [-f]
```

## How to Use the Project
```
./Channel_Selection.sh -p <phyname> [-s] [-f]
p   => The Specific phy interface that you want to run the algorithm.
Should give the phy name that you can see by using iw dev.
By default phy0 => 2.4GHz, phy1 => 5GHz.

s   => To set the sleep interval between the channel hopping. Default 1s
Should input positive integer.

f   => To set the channel hopping loop time. Default 1 time
Should input positive integer.
```
* During the algorithm all of your virtual interfaces except monitor on the indicated phy interface will be disable temporary. After the algorithm finish it will resume all the virtual interfaces.
