#!/bin/ash
#
# 在還沒進行 Channel_Selection時不斷地 monitor當前的 channel，並且用 AVG計算(或是網工課，拿來計算 delay還什麼的那ˋ些方法)
# 當 Channel_Selection結束時，則跟掃出來的結果做計算，就能取得我們的系統地消耗大概佔比多少，
# 以此讓頻道之間的比較不會因為我們系統的負擔造成錯誤的比對。
#
#

check=`tcpdump --version`
res=$?

if [ $res -ne 0 ]; then
    echo "Please install tcpdump!"
    exit 0
fi

cd ~/Monitor
logFile="logMonitor"
echo "----------monitor.sh----------" >> "${logFile}"

moni_itf=""
target_itf=""
si=10
while getopts m:t:s: flag
do
    case "${flag}" in
        m) moni_itf=${OPTARG};;
        t) target_itf=${OPTARG};;
        s) si=${OPTARG};;
    esac
done

# The p tag phy should be needed
if [ "${moni_itf}" == "" ]; then
    echo "Please give monitor interface!"
    exit 0
fi

if [ "${target_itf}" == "" ]; then
    echo "Please give target interface!"
    exit 0
fi

target_addr=`awk -f Get_Interface_Addr.awk "${target_itf}"`
echo ${target_addr}

result=`awk -f monitor.awk "${moni_itf}" "${target_addr}" "${si}"` & ./countdown.sh "${si}" "${target_addr}"
wait

echo ${result}

echo "----------monitor.sh----------" >> "${logFile}"
echo "Monitor Finish!!" | tee -a "${logFile}"
