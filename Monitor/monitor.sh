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

path=`pwd`
cd "${path}"
logFile="logMonitor"
echo "----------Start monitor.sh----------" >> "${logFile}"
echo cd "${path}" >> "${logFile}"

tmp=""
moni_itf=""
target=""
si=5
myflag=""
writeFile=""
writeFlag=0

# put ':' befind char if there are some  input parameters come with that char
while getopts ":c:s:i:m:w:tvba" flag
do
    case "${flag}" in
        c) moni_itf=${OPTARG};;
        s) si=${OPTARG};;
        i) target=${OPTARG};;
        m) 
            tmp=${OPTARG}
            target=`awk -f Get_Interface_Addr.awk "${tmp}"`
            ;;
        t) 
            myflag="t"
            # Scan for specific target
            ;;
        v)
            myflag="v"
            # Scan except specific target
            ;;
        b)
            myflag="b"
            # Scan everything but seperate target and untarget
            ;;
        a)
            myflag="a"
            # Scan everything
            ;;
        w)
            writeFile=${OPTARG}
            writeFlag=1
            ;;
    esac
done

# The p tag phy should be needed
if [ "${moni_itf}" == "" ]; then
    echo "Please give monitor interface!"
    exit 0
fi

echo awk -f monitor.awk "${moni_itf}" "${target}" "${si}" "${myflag}" >> "${logFile}"
result=`awk -f monitor.awk "${moni_itf}" "${target}" "${si}" "${myflag}"`
wait

if [ $writeFlag -eq 1 ]; then
    # echo "${now}" >> "${writeFile}"
    echo "${result}" >> "${writeFile}"
else
    echo "${result}"
fi

echo "Monitor Finish!!" | tee -a "${logFile}"
echo "----------End monitor.sh----------" >> "${logFile}"
