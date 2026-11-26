#!/bin/bash

powermode=$1
mtype=$2

function set_n8p {
        # n8p NIC 0x42=L1 0x140=disable
        pcinum=`lspci | grep "Ethernet controller" | awk -F" " '{print $1}'`
        powermode=$1
        nicmode="0x140"
        if [ "x$powermode" = "xdim" ]; then
            nicmode="0x42"
        elif [ "x$powermode" = "xperformance" ]; then
            nicmode="0x140"
        elif [ "x$powermode" = "xpowersave" ]; then
            nicmode="0x42"
        elif [ "x$powermode" = "xpowersupersave" ]; then
            nicmode="0x42"
        fi

        if [ "x$pcinum" != "x" ]; then
            setpci -s $pcinum CAP_EXP+10.w=$nicmode
        fi
}

if [ "x$powermode" != "x" ]; then
 
    boardname=`cat /sys/devices/virtual/dmi/id/board_name`
    if [ "x$boardname" = "xLXKT-ZXG-N8P" ]; then
        set_n8p $powermode
    fi

fi
# "  powersave  /sys/module/pcie_aspm/parameters/policy"
