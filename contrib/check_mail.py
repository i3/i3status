#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script wraps i3status output and inject mail status, or just reports
# status on command line.  It is based on 'wrapper.py' script by Valentin
# Haenel, which could be found at:
# http://code.stapelberg.de/git/i3status/tree/contrib/wrapper.py
#
# To use it, ensure your ~/.i3status.conf contains this line:
#     output_format = "i3bar"
# in the 'general' section.
# Then, in your ~/.i3/config, use:
#     status_command i3status | path/to/check_mail.py ...
# In the 'bar' section.
#
# Or just run:
#     ./check_mail.py -1 ...
#
# Information on command line arguments (flags) may be obtained by running
#     ./check_mail.py -h
#
# © 2015 Dmitrij D. Czarkoff <czarkoff@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

import argparse
import json
import os
import os.path
import sys

def print_line(message):
    """ Non-buffered printing to stdout. """
    sys.stdout.write(message + '\n')
    sys.stdout.flush()

def read_line():
    """ Interrupted respecting reader for stdin. """
    # try reading a line, removing any extra whitespace
    try:
        line = sys.stdin.readline().strip()
        # i3status sends EOF, or an empty line
        if not line:
            sys.exit(3)
        return line
    # exit on ctrl-c
    except KeyboardInterrupt:
        sys.exit()

def check_mail(label, nomail, ignore, colors):
    """ Check mail in mailbox "label" and return report and color """
    name = label
    if not os.path.isabs(name):
        name = os.path.join(os.environ['HOME'], name)

    if not os.path.exists(name) or os.path.getsize(name) == 0:
        return nomail, colors[0]

    if os.path.isfile(name):
        import mailbox

        try:
            mbox = mailbox.mbox(name, create = False)
            messages = 0
            for msg in mbox:
                if msg.get_flags() == '':
                    messages += 1

            if messages > 0:
                return '{0}:{1}'.format(os.path.basename(name),
                        messages), colors[1]
            else:
                return nomail, colors[0]

        except:
            pass

    elif os.path.isdir(name):
        report = ''
        maildirs = 0

        for subdir in os.listdir(name):
            subdirpath = os.path.join(name, subdir)
            if 'new' in os.listdir(subdirpath):
                maildirs += 1
                if subdir not in ignore:
                    messages = len(os.listdir(os.path.join(subdirpath, 'new')))
                    if messages > 0:
                        report += ' {0}:{1}'.format(subdir, messages)

        if maildirs > 0:
            if len(report) > 0:
                return report[1:], colors[1]
            else:
                return nomail, colors[0]

    return '{0}: not a mailbox'.format(name), colors[2]

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Check mailboxes.')
    parser.add_argument('mailbox', help='path to mailbox', type=str, nargs='*',
            default=['/var/mail/{0}'.format(os.getlogin())])
    parser.add_argument('-i', '--ignore', action='append', help='ignore named '
            'mailboxes', type=str, dest='ignore', metavar='mailbox')
    parser.add_argument('-1', '--once', help='check mailboxes, write results '
            'to stdout and exit', action='store_true')
    parser.add_argument('-0', '--nomail', help='text to print if no new mail '
            'found', type=str, default='')
    parser.add_argument('-g', '--good', help='color to use when there is no '
            'new mail', type=str, default='#00FF00')
    parser.add_argument('-d', '--degraded', help='color to use when there is '
            'new mail', type=str, default='#FFFF00')
    parser.add_argument('-b', '--bad', help='color to use when error was '
            'detected', type=str, default='#FF0000')
    parser.add_argument('-p', '--position', help='position of mail reports in '
            'i3bar status', type=int, default=0)
    args = parser.parse_args()

    colors = [args.good, args.degraded, args.bad]

    if args.once:
        for name in args.mailbox:
            report, color = check_mail(name, args.nomail, args.ignore, colors)
            sys.stderr.write(report + '\n')
        exit(0)

    # Skip the first line which contains the version header.
    print_line(read_line())

    # The second line contains the start of the infinite array.
    print_line(read_line())

    while True:
        line, prefix = read_line(), ''
        # ignore comma at start of lines
        if line.startswith(','):
            line, prefix = line[1:], ','

        j = json.loads(line)
        # insert information into the start of the json
        i = args.position
        for name in args.mailbox:
            report, color = check_mail(name, args.nomail, args.ignore, colors)
            j.insert(i, {
                    'name' : 'mail',
                    'instance' : name,
                    'full_text' : report,
                    'color' : color
                        })
            i += 1
        # and echo back new encoded json
        print_line(prefix+json.dumps(j))
