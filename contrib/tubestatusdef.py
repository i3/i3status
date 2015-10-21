#!/usr/bin/python3

#
# Copyright:   Conor O'Callghan 2015
# Version:     v1.1.1
# 
# Please feel free to fork this project, modify the code and improve 
# it on the github repo https://github.com/brioscaibriste/iarnrod 
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
import tempfile
import time
import os
from urllib.request import urlopen

'''

ParseArgs

A simple function to parse the command line arguments passed to the function. 
The function does very little sanitisation on the input variables. The 
argument passed is then returned from the function.  

'''

def ParseArgs():

    # Parse our command line argument for the line name
    parser = argparse.ArgumentParser()
    parser.add_argument('--line',dest='LineName',help='Specify the London line you want to report on')
    args = parser.parse_args()

    # Check if the value is blank
    Line = (args.LineName)
    
    if not Line:
        print ("\nError, you must specify a line name! e.g. --line district\n")
        sys.exit(1)
    
    # Convert the line name to lower case for easy comparison
    Line = Line.lower()

    # If the line isn't in the line list, fail badly
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

    # Convert the tube line back to upper case for nice display
    Line = Line.upper()

    return Line    

'''

RetrieveTFLData

This function takes the Line variable (a name of a Transport For London line 
name) and polls the TFL API. The function then returns the current line
status for the specified line. 

'''

def RetrieveTFLData(Line,Run,SFileName):

    # TFL Unified API URL
    TFLDataURL = "https://api.tfl.gov.uk/Line/" + Line + ("/Status?detail=False"
    "&app_id=&app_key=")

    if Run: 
        # Read all the information from JSON at the specified URL
        RawData = urlopen(TFLDataURL).readall().decode('utf8') or die("Error, failed to "
                "retrieve the data from the TFL website")
        TFLData = json.loads(RawData)

        # Sanitize the data to get the line status
        Scratch = (TFLData[0]['lineStatuses'])
        LineStatusData = (Scratch[0]['statusSeverityDescription'])
        
        # Cache the staus in a file
        with open(SFileName, 'w+') as SFile:
            SFile.write(LineStatusData)
        SFile.closed
      
    else:
        with open(SFileName, 'r+') as SFile:
            LineStatusData = SFile.read()
        SFile.closed

    return LineStatusData

'''

 check if temp file exists, if not, write it with time stamp in it, else pull time stamp
 check time stamp vs. poll interval, if it's less than time + interval, skip it, else run and write the temp file again
 two functions at least, one for time, one for polling, returning status value
'''

def Throttle(PollInterval,Throttle,TFileName):

    # Does the temporary file exist
    TFileStatus = os.path.isfile(TFileName)
    
    # Current epoch time
    CurrentStamp = str(time.time()).split('.')[0]

    if TFileStatus: 

        with open(TFileName, 'r+') as TFile:
            TimeFile = TFile.read()
        TFile.closed
        
        Remainder = int(CurrentStamp) - int(TimeFile)
    
    else:
        # Get the current time stamp and write it to the temp file
        with open(TFileName, 'w+') as TFile:
            TFile.write(CurrentStamp)
        TFile.closed
 
        # Set the Remainder high to force the next run
        Remainder = 1000000 

    # The poll interval in seconds
    PollInterval = PollInterval * 60

    # If the remainder is less than the poll interval don't run the command, if it isn't run the command
    if ( Remainder < PollInterval ):
        Run = 0
    else:
        Run = 1
        
        # Set the command to run and re-write the poll time to file
        with open(TFileName, 'w+') as TFile:
            TFile.write(CurrentStamp)
        TFile.closed

    return Run
