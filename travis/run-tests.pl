#!/usr/bin/env perl

use v5.10;
use strict;
use warnings;
use English;
use Term::ANSIColor qw(:constants);
use File::Basename;

sub TestCase {
    my ($dir) = @_;

    if ( -f "@_/setup.pl") {
        system($EXECUTABLE_NAME, "@_/setup.pl", ($dir));
    }

    my $conf = "$dir/i3status.conf";
    my $testres = `./i3status --run-once -c $conf`;
    my $exitcode = $?;
    my $refres = "";
    my $fh;

    if ( -f "@_/expected_output.txt") {
        open($fh, '<:encoding(iso-8859-1)', "@_/expected_output.txt")
          or die "Could not open file '@_/expected_output.txt' $!";
        local $/ = undef; # <--- slurp mode
        $refres = <$fh>;
        close($fh);
    } elsif ( -f "@_/expected_output.pl") {
        $refres = `$EXECUTABLE_NAME @_/expected_output.pl`;
    }

    if ( -f "@_/cleanup.pl") {
        system($EXECUTABLE_NAME, "@_/cleanup.pl", ($dir));
    }

    if ( $exitcode != 0 ) {
        say "Testing test case '", basename($dir), "'… ", BOLD, RED, "Crash!", RESET;
        return 0;
    }

    if ( "$testres" eq "$refres" ) {
        say "Testing test case '", basename($dir), "'… ", BOLD, GREEN, "OK", RESET;
        return 1;
    } else {
        say "Testing test case '", basename($dir), "'… ", BOLD, RED, "Failed!", RESET;
        say "Expected: '$refres'";
        say "Got: '$testres'";
        return 0;
    }
}

my $testcases = 'testcases';
my $testresults = 0;

opendir(my $dir, $testcases) or die "Could not open directory $testcases: $!";

while (my $entry = readdir($dir)) {
    next unless (-d "$testcases/$entry");
    next if ($entry =~ m/^\./);
    if (not TestCase("$testcases/$entry") ) {
        $testresults = 1;
    }
}
closedir($dir);
exit $testresults;
