#!/bin/bash

confile="/usr/share/found-monitors-switcher/hotplug_conf"
[ ! -e $confile ] && sudo touch $confile && sudo chmod 666 $confile

while true
do
flag=`cat $confile`
[ $flag -eq 1 ] && echo -n "2" > $confile && . /usr/share/found-monitors-switcher/switcher_mode 
sleep 1
done
