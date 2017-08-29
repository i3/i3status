#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
if ($cpu_count == 1) {
    print "all: 100% CPU_0: 100% CPU_1: \n";
} else {
    print "all: 75% CPU_0: 100% CPU_1: 50%\n";
}
