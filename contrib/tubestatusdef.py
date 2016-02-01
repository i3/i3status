#!/usr/bin/python3

#
# Copyright:   Conor O'Callghan 2016
# Version:     v1.1.3
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

Inputs: 

Line      - Which line to retrieve information on  
Run       - Should the data retrieval be run or should the cache file be used
SFileName - The file in which to store the line status cache 

This function takes the Line variable (a name of a Transport For London line 
name) and polls the TFL API. The function then returns the current line
status for the specified line. 

'''

def RetrieveTFLData(Line,Run,SFileName):

    # TFL Unified API URL
    TFLDataURL = "https://api.tfl.gov.uk/Line/" + Line + ("/Status?detail=False"
    "&app_id=&app_key=")

    if Run: 
        # Read all the information from JSON at the specified URL, can be re-done with requests?
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

Throttle

Inputs 

  PollIntervalMinutes - Polling interval in minutes
  Throttle            - Should we throttle the connection or not?
  TFileName           - The file where the timestamp for throttling usage is stored 

This function is used to determine whether or not the next run of the retrieval of data should run.
It retrieves the previously run time from a file in /tmp if it exists, if the file does not exist
the run status will return as 1 and the current time stamp will be written into a new file. 

If throttling is disabled, the file will be removed from /tmp and run will be set to 1. 
'''

def Throttle(PollIntervalMinutes,Throttling,TFileName):

    if Throttling == "True":
    
        # Current epoch time
        CurrentStamp = str(time.time()).split('.')[0]
    
        # Does the temporary file exist or not
        if os.path.isfile(TFileName): 
    
            # Open the temp file and read the time stamp
            with open(TFileName, 'r+') as TFile:
                TimeFile = TFile.read()
            
            Remainder = int(CurrentStamp) - int(TimeFile)
        
        else:
            # Get the current time stamp and write it to the temp file
            with open(TFileName, 'w+') as TFile:
                TFile.write(CurrentStamp)
     
            # Set the Remainder high to force the next run
            Remainder = 1000000 
    
        # If the remainder is less than the poll interval don't run the command, if it isn't run the command
        if ( Remainder < (PollIntervalMinutes * 60) ):
            Run = 0
        else:
            Run = 1
            
            # Set the command to run and re-write the poll time to file
            with open(TFileName, 'w+') as TFile:
                TFile.write(CurrentStamp)
    
        return Run

    else:
        # Remove the time file if it exists
        try:
            os.remove(TFileName)
        except OSError:
            pass

        Run = 1
        return Run
