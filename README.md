# i3status 

## Description

i3status is a small program (about 1500 SLOC) for generating a status bar for
i3bar, dzen2, xmobar or similar programs. It is designed to be very efficient by
issuing a very small number of system calls, as one generally wants to update
such a status line every second. This ensures that even under high load, your
status bar is updated correctly. Also, it saves a bit of energy by not hogging
your CPU as much as spawning the corresponding amount of shell commands would.

## Development

i3status has the following dependencies:
  * libconfuse-dev
  * libyajl-dev
  * libasound2-dev
  * libnl-genl-3-dev
  * libcap2-bin (for getting network status without root permissions)
  * asciidoc (only for the documentation)
  * libpulse-dev (for getting the current volume using PulseAudio)

On debian-based systems, the following line will install all requirements:
```bash
apt-get install libconfuse-dev libyajl-dev libasound2-dev libiw-dev asciidoc libcap2-bin libpulse-dev libnl-genl-3-dev
```

## Upstream

i3status is developed at https://github.com/i3/i3status

## Compilation 

Compiling is done with the usual make-line
```bash
  make && sudo make install
```
