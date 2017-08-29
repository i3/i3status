#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
if ($cpu_count == 1) {
    print "all: 00% CPU_0: 00% CPU_1: \n";
} else {
    print "all: 50% CPU_0: 00% CPU_1: 100%\n";
}
