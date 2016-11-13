#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;
use Term::ANSIColor qw(:constants);
use File::Basename;

sub TestCase {
    my ($dir) = @_;
    my $conf = "$dir/i3status.conf";
    my $testres = `./i3status --run-once -c $conf`;
    my $refres = "";

    if ( -f "@_/expected_output.txt") {
        $refres = `cat "@_/expected_output.txt"`;
    } elsif ( -f "@_/expected_output.sh") {
        $refres = `bash @_/expected_output.sh`;
    }

    if ( "$testres" eq "$refres" ) {
        say "Testing test case '", basename($dir), "'… ", BOLD, GREEN, "OK", RESET;
        return 1;
    } else {
        say "Testing test case '", basename($dir), "'… ", BOLD, RED, "Failed!", RESET;
        return 0;
    }
}


my $testcases = 'testcases';
my $testresults = 1;

opendir(my $dir, $testcases) or die "Could not open directory $testcases: $!";

while (my $entry = readdir($dir)) {
    next unless (-d "$testcases/$entry");
    next if ($entry =~ m/^\./);
    $testresults = $testresults && TestCase("$testcases/$entry");
}
closedir($dir);
exit 0;
