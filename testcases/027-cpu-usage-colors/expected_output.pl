#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

# Every value is colored on its own: the overall usage is degraded, the first
# CPU is above the max threshold and the second CPU is below both thresholds.
chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
if ($cpu_count == 1) {
    print "all: <fc=#FF0000>100%</fc> CPU_0: <fc=#FF0000>100%</fc> CPU_1: \n";
} else {
    print "all: <fc=#FFFF00>75%</fc> CPU_0: <fc=#FF0000>100%</fc> CPU_1: 50%\n";
}
