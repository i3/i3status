#!/bin/bash
# Public Domain
# (someone claimed the next lines would be useful for…
#  people. So here goes: © 2012 Stefan Breunig
#  stefan+measure-net-speed@mathphys.fsk.uni-heidelberg.de)


# path to store the old results in
path="/dev/shm/measure-net-speed"

# grabbing data for each adapter. 
# You can find the paths to your adapters using
#  find /sys/devices -name statistics
# If you have more (or less) than two adapters, simply adjust the script here
# and in the next block. 
eth0="/sys/devices/pci0000:00/0000:00:19.0/net/eth0/statistics"
wlan0="/sys/devices/pci0000:00/0000:00:1c.1/0000:03:00.0/net/wlan0/statistics"
read eth0_rx < "${eth0}/rx_bytes"
read eth0_tx < "${eth0}/tx_bytes"
read wlan0_rx < "${wlan0}/rx_bytes"
read wlan0_tx < "${wlan0}/tx_bytes"

# get time and sum of rx/tx for combined display
time=$(date +%s)
rx=$(( $eth0_rx + $wlan0_rx ))
tx=$(( $eth0_tx + $wlan0_tx ))

# write current data if file does not exist. Do not exit, this will cause
# problems if this file is sourced instead of executed as another process.
if ! [[ -f "${path}" ]]; then
  echo "${time} ${rx} ${tx}" > "${path}"
  chmod 0666 "${path}"
fi

# read previous state and update data storage
read old < "${path}"
echo "${time} ${rx} ${tx}" > "${path}"

# parse old data and calc time passed
old=(${old//;/ })
time_diff=$(( $time - ${old[0]} ))

# sanity check: has a positive amount of time passed
if [[ "${time_diff}" -gt 0 ]]; then
  # calc bytes transferred, and their rate in byte/s
  rx_diff=$(( $rx - ${old[1]} ))
  tx_diff=$(( $tx - ${old[2]} ))
  rx_rate=$(( $rx_diff / $time_diff ))
  tx_rate=$(( $tx_diff / $time_diff ))

  # shift by 10 bytes to get KiB/s. If the value is larger than
  # 1024^2 = 1048576, then display MiB/s instead (simply cut off  
  # the last two digits of KiB/s). Since the values only give an  
  # rough estimate anyway, this improper rounding is negligible.

  # incoming
  rx_kib=$(( $rx_rate >> 10 ))
  if [[ "$rx_rate" -gt 1048576 ]]; then
    echo -n "${rx_kib:0:-3}.${rx_kib: -3:-2} M↓"
  else
    echo -n "${rx_kib} K↓"
  fi

  echo -n "  "

  # outgoing
  tx_kib=$(( $tx_rate >> 10 ))
  if [[ "$tx_rate" -gt 1048576 ]]; then
    echo -n "${tx_kib:0:-3}.${tx_kib: -3:-2} M↑"
  else
    echo -n "${tx_kib} K↑"
  fi
else
  echo -n " ? "
fi