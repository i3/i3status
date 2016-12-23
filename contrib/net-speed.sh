#!/bin/sh

# Authors:
# - Moritz Warning <moritzwarning@web.de> (2016)
# - Zhong Jianxin <azuwis@gmail.com> (2014)
#
# See file LICENSE at the project root directory for license information.
#
# i3status.conf should contain:
# general {
#   output_format = i3bar
# }
#
# i3 config looks like this:
# bar {
#   status_command exec /usr/share/doc/i3status/contrib/net-speed.sh
# }

# Auto detect interfaces
ifaces=$(ls /sys/class/net | grep -E '^(eth|wlan|enp|wlp)')

last_time=0
last_rx=0
last_tx=0
rate=""

readable() {
  local bytes=$1
  local kib=$(( bytes >> 10 ))
  if [ "$kib" -gt 1024 ]; then
    local mib_int=$(( kib >> 10 ))
    local mib_dec=$(( kib % 1024 * 976 / 10000 ))
    if [ "$mib_dec" -lt 10 ]; then
      mib_dec="0${mib_dec}"
    fi
    echo "${mib_int}.${mib_dec} M"
  else
    echo "${kib} K"
  fi
}

update_rate() {
  local time=$(date +%s)
  local rx=0 tx=0 tmp_rx tmp_tx

  for iface in $ifaces; do
    read tmp_rx < "/sys/class/net/${iface}/statistics/rx_bytes"
    read tmp_tx < "/sys/class/net/${iface}/statistics/tx_bytes"
    rx=$(( rx + tmp_rx ))
    tx=$(( tx + tmp_tx ))
  done

  local interval=$(( $time - $last_time ))
  rate="$(readable $(( (rx - last_rx) / interval )))↓ $(readable $(( (tx - last_tx) / interval )))↑"
  last_time=$time
  last_rx=$rx
  last_tx=$tx
}

i3status | (read line && echo "$line" && read line && echo "$line" && read line && echo "$line" && update_rate && while :
do
  read line
  update_rate
  echo ",[{\"full_text\":\"${rate}\" },${line#,\[}" || exit 1
done)
