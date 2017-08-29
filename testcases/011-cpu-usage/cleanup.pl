#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;

if ($#ARGV != 0 || ! -d $ARGV[0]) {
    say "Error with cleanup script: argument not provided or not a directory";
    exit 1;
}

my $output_file = "$ARGV[0]/stat";
if (-f $output_file) {
    unlink $output_file;
}
