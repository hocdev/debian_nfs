#!/bin/bash

aecArgs="$*"
# If no "aecArgs" are passed on to this shell script, use this "aec_args" as default:
[ -z "$aecArgs" ] && aecArgs="analog_gain_control=0 digital_gain_control=1"

markEchoCancelSource=".echoCancelSource"
defaultSource=$(pacmd stat | grep "Default source name" | cut -d ":" -f2 | tr -d ' ')

if [[ $defaultSource == *"$markEchoCancelSource"* ]]; then
  newEchoCancelSource=$defaultSource
  newSource=${defaultSource%"$markEchoCancelSource"}
else
  newEchoCancelSource=$defaultSource$markEchoCancelSource
  newSource=$defaultSource
fi

# Re-Activate module-echo-cancel submodule
echo Re-Activate \"module-echo-cancel\" with \"aec_args=$aecArgs\"
pactl unload-module module-echo-cancel 2>/dev/null
pactl load-module module-echo-cancel use_master_format=1 aec_method=webrtc aec_args=\"$aecArgs\" source_name=$newEchoCancelSource
