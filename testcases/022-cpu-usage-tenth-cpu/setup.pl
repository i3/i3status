#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

if ($#ARGV != 0 || ! -d $ARGV[0]) {
    say "Error with setup script: argument not provided or not a directory";
    exit 1;
}

chomp(my $cpu_count = `grep -c -P '^processor\\s+:' /proc/cpuinfo`);
my $output_file = "$ARGV[0]/stat";
open(my $fh, '>', $output_file) or die "Could not open file '$output_file' $!";
print $fh "cpu  0 0 0 0 0 0 0 0 0 0\n";
for (my $i = 0; $i < $cpu_count; $i++) {
    print $fh "cpu$i 0 0 0 0 0 0 0 0 0 0\n";
}
close $fh;
