#!/usr/bin/python3

# Iarnród 
#
# Copyright:   Conor O'Callghan 2015
# Version:     v1.0.1
# 
# Please feel free to fork this project, modify the code and improve 
# it on the github repo https://github.com/brioscaibriste/iarnrod 

# i3status
#
# This script works with i3status, though probably not as it should :[
#
# In order to make this work with the i3status, you should modify your
# .i3status.conf and ensure it contains the following line
#
#   output_format = "i3bar"
#
# In the general section of your configuration you should have somthing similar
# to the following
#
#    status_command i3customstatus
#
# As per the i3status documentation, you should add a wrapper accessible on your
# path which executes drawing in the i3status details alongside this script. 
# A sample wrapper file can be found in the man page for i3status.

# License
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import json
import sys
from urllib.request import urlopen

# Parse our command line argument for the line name
parser = argparse.ArgumentParser()
parser.add_argument('--line',dest='LineName',help='Specify the London line you want to report on')
args = parser.parse_args()

# Convert the line name to lower case for easy comparison
Line = (args.LineName)
Line = Line.lower()

if Line not in ('district','circle','victoria','central','northern',
     'bakerloo','hammersmith-city','jubilee','metropolitan', 
     'piccadilly','waterloo-city','dlr',):
     print ("\nError, you have specified " + Line + " as your line. You must specify one of the following: "
            "\n\tDistrict"
            "\n\tCircle"
            "\n\tVictora"
            "\n\tCentral"
            "\n\tNorthern"
            "\n\tPiccadilly"
            "\n\tBakerloo"
            "\n\thammersmith-city"
            "\n\twaterloo-city"
            "\n\tDLR"
            "\n\tMetropolitan"
            "\n\tJubilee\n")
     sys.exit(1)

# You can set this to small or large and it will change the output format
StatusOutput = "small"

# TFL Unified API URL
TFLDataURL = "https://api.tfl.gov.uk/Line/" + Line + ("/Status?detail=False"
    "&app_id=&app_key=")

# Read all the information from JSON at the specified URL
RawData = urlopen(TFLDataURL).readall().decode('utf8') or die("Error, failed to "
    "retrieve the data from the TFL website")
TFLData = json.loads(RawData)

# Sanitize the data to get the line status
Scratch = (TFLData[0]['lineStatuses'])
LineStatusData = (Scratch[0]['statusSeverityDescription'])

# Convert the tube line back to upper case for nice display
Line = Line.upper()

# Generate the status output and print
if (StatusOutput == "small") and (LineStatusData == "Good Service"):
    print ("TFL " + Line[0] + Line[1] + Line[2] + " OK")
elif StatusOutput == "small":
    print ("TFL " + Line[0] + Line[1] + Line[2] + " " + LineStatusData)
else:
    print ("TFL " + Line + " line has " + LineStatusData)
