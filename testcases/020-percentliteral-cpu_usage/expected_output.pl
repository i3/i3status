#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
if ($cpu_count == 1) {
    print "I can %haz literal% % ? %00%%\n";
} else {
    print "I can %haz literal% % ? %50%%\n";
}
