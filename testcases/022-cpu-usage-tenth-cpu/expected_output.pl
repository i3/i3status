#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
if ($cpu_count < 10) {
    print "all: 00% CPU_0: 00% CPU_10: \n";
} else {
    print "all: 00% CPU_0: 00% CPU_10: 00%\n";
}
