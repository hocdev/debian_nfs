#! /bin/bash

# 禁用摄像头
disable_camera () {
    # 在这里杀死进程会消耗太长时间
    # pid=`lsof | grep video0 | awk '{print $2}' | sed -n '1p'`
    # [ ! -z "$pid" ] && kill -9 $pid
    echo "blacklist uvcvideo" >> /etc/modprobe.d/blacklist.conf
    # sleep 0.2
    modprobe -r uvcvideo
}

# 启动摄像头
enable_camera () {
    modprobe -a uvcvideo
    sed -i "/blacklist uvcvideo/d" /etc/modprobe.d/blacklist.conf 
}

# 设置摄像头开关状态
set_camera_status () {
    if [ "$1" = "disable" ]
    then
        disable_camera
    elif [ "$1" = "enable" ]
    then
        enable_camera
    else
        echo "Please input right parameter"
        exit 1
    fi
}

set_camera_status $1