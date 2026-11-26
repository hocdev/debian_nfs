#!/bin/bash -e
#echo "$1 $2"  >> /tmp/log.txt 

if [ "X$1" != "X" -a "X$2" != "X" ]; then
	#echo $1 $2
	#bestres1=`xrandr | grep -e '-' -e '+' | awk '{print $1}'| grep -A1 $1 | grep -v $1 | grep 'x'`
	#bestres1=`xrandr | grep -e '-' -e '+' | awk '{print $1}' | sed -n '/'${1}'/{n;p}'`
	i=0
	arr=()
	for line in `xrandr | grep -e '-' -e '+' | awk '{print $1}'`
	do
		arr[$i]=$line
		i=`expr $i + 1`
		#echo $line
		#echo " $line | grep -q $1 && r1=$i"
		echo $line | grep -q $1 && r1=$i
		echo $line | grep -q $2 && r2=$i
	done
	bestres1=`echo ${arr[$r1]} | grep x`
	bestres2=`echo ${arr[$r2]} | grep x`
	#echo "------$r1  $r2 ----${bestres1}----${bestres2}-----="
	bestres2=`xrandr | grep -e '-' -e '+' | awk '{print $1}' | grep -A1 $2 | grep -v $2 | grep 'x'`
	quirk=`lspci -nn | grep 8086:0166`
	bestres3=`xrandr | grep '1920x1080i'`
	#echo "----------${bestres1}----${bestres2}-----="
	if [ "X$quirk" != "X" -a "X$bestres3" != "X" ]; then
		echo $1 | grep -q -i HDMI  && echo "1920x1080i|$bestres2" && exit 0 
		echo $2 | grep -q -i HDMI  && echo "${bestres1}|1920x1080i" && exit 0
	fi
	#echo "----------${bestres1}----${bestres2}-----"
	#bestres2=`xrandr |grep -e '-' -e '+' |  awk -F" " '{print $2}' | sed -n '/$1/{n;p}' | grep 'x'`
	echo "${bestres1}|${bestres2}"
	exit 0
fi
if [ "X$1" != "X" ]; then

	i=0
	arr=()
	for line in `xrandr | grep -e '-' -e '+' | awk '{print $1}'`
	do
		arr[$i]=$line
		i=`expr $i + 1`
		#echo $line
		#echo " $line | grep -q $1 && r1=$i"
		echo $line | grep -q $1 && r1=$i
	done
	bestres=`echo ${arr[$r1]} | grep x`
	if [ "X$bestres" != "X" ]; then
	   quirk=`lspci -nn | grep 8086:0166`
	   bestres3=`xrandr | grep '1920x1080i'`
	   if [ "X$quirk" != "X" -a "X$bestres3" != "X" ]; then
		echo $1 | grep -q -i HDMI  && echo "1920x1080i" && exit 0 
	   fi
	   echo $bestres
        fi	       
	exit 0
fi


i=0
#get all resolution
#file="/usr/lib/cdos-multimonitor-switcher/a"
#for line in `cat $file `
for line in `xrandr `
do
	echo $line | grep -qe '^[0-9]\+x[0-9]\+$' && arr[$i]=$line &&  i=`expr $i + 1`
done

count=${#arr[@]}

#realign arrdig with descend order
#1.compute digital resolution
temp=0
for((i=0;i<$count;i++))
do
	left=` echo ${arr[$i]} | awk -F "x" '{print $1}'`
	right=` echo ${arr[$i]} | awk -F "x" '{print $2}'`
	arrdig[$i]=`expr $left \* $right`
done

#1-1.deal with 1360x768 external monitor specially
#for((i=0;i<$count;i++))
#do
#	if [ "x${arr[$i]}" == "x1360x768" ] ;then
#		echo "1366x768|1360x768" && exit 0
#	fi
#done

#2.resort digital array with descend order
for((i=0;i<$count;i++))
do
	left=${arrdig[$i]}
	
	for((j=`expr $i+1`;j<$count;j++))
	do
		right=${arrdig[$j]}
		if [ $left -lt $right ]; then
			temp=${arrdig[$i]}
			arrdig[$i]=${arrdig[$j]}
			arrdig[$j]=$temp
			left=$right
		fi
	done
done

#3.find target digital : the biggest one that found in twice
for ((i=0;i<$count;i++))
do
	
	left=${arrdig[$i]}
	right=${arrdig[$i+1]}
	#echo "$left -> $right"
	if [ "x$left" == "x$right" ];then
		target=$left && break
	fi
done
#echo "Target: $target"

#4.find the resolution which is the target digital
#1360x768 handle
sflag=0
s1366=0
s1360=0
starget=0
for((i=0;i<$count;i++))
do
	left=` echo ${arr[$i]} | awk -F "x" '{print $1}'`
	right=` echo ${arr[$i]} | awk -F "x" '{print $2}'`
	t=`expr $left \* $right`
	if [ "x$target" == "x$t" ];then
		#echo "${arr[$i]}|${arr[$i]}" && exit 0
		starget="${arr[$i]}|${arr[$i]}"
	        if [ $left -gt 1000 ]; then
		   sflag=1
		   echo "${arr[$i]}|${arr[$i]}" && exit 0
	        fi
	fi
	if [ $left -gt 1300 ]; then
	   if [ "X${arr[$i]}" = "X1360x768" ]; then
		s1360=1
	   fi
	   if [ "X${arr[$i]}" = "X1366x768" ]; then
		s1366=1
	   fi
	fi
	#if [ "X$sflag" = "X0" ]; then
	   if [ "X$s1366" = "X1" -a "X$s1360" = "X1" ]; then
	       echo "1366x768|1360x768" && exit 0
	   fi    
	#fi

done
