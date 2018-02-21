#!/bin/bash
# This script adds an external IP address to i3status for i3bar and
# could be used to add other content while preserving color functionality. 
# Requires unbuffer from the expect package on Arch Linux.
# 2018 Evan Friedley, Public Domain

# /etc/i3status.conf should contain:
# general {
#   output_format = "i3bar"
# }

# ~/.config/i3/config (or other i3 conf) should contain:
# bar {
#   status_command exec /path/to/ipstatus.sh
# }

unbuffer i3status | while :    #preserve color
do
    read line
    line=`echo $line | sed 's/\[//'`    #remove first bracket
    ip=`curl -s icanhazip.com`    #grab something
    format="[{\"full_text\":\"$ip \"}"			
    final=`echo $format,$line, | sed 's/,,/,/'`    #fix JSON formatting
    echo $final || exit 1 
done
