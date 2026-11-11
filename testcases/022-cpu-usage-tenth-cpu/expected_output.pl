#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
if ($cpu_count < 10) {
    print "all:   0% CPU_0:   0% CPU_10: \n";
} else {
    print "all:   0% CPU_0:   0% CPU_10:   0%\n";
}
