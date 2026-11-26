#!/bin/bash -e
#本脚本仅由切屏脚本在“仅计算机模式”时调用

#获得内部模拟音频槽id
analog_sinkid=`pacmd  list-sinks | grep index -A 1  | grep analog -B 2 | head -2 | grep index | awk -F " " '{print $NF}'`


#设置内部音频槽id为默认音频槽
pacmd set-default-sink $analog_sinkid

#将所有stream 从内部音频槽输出
for sinput in `pacmd list-sink-inputs | grep index | awk -F " " '{print $2}'`
do
	pacmd move-sink-input $sinput $analog_sinkid
done

