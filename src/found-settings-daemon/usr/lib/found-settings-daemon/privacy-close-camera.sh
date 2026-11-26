#! /bin/bash

pid=`lsof | grep video0 | awk '{print $2}' | sed -n '1p'`
[ ! -z "$pid" ] && kill -9 $pid
sleep 0.2
modprobe -r uvcvideo