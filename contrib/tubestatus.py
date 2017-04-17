#!/usr/bin/python3

#
# Copyright:   Conor O'Callghan 2016
# Version:     v1.1.3
# 
# Please feel free to fork this project, modify the code and improve 
# it on the github repo https://github.com/brioscaibriste/iarnrod 
#
# Powered by TfL Open Data	
#
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

import tubestatusdef

# Iarnrod Tuning options
Throttling = "True" # Limit the burden on the API, throttle the connections
PollIntervalMinutes = 5 # This is the status polling interval in minutes  
StatusOutput = "small" # You can set this to small or large and it will change the output format
TFileName = '/tmp/iarn-i3-temp' # Where to store the timestamp file for poll throttling
SFileName = '/tmp/iarn-i3-stat' # Where to store the cache of the line status

# Parse the command line arguments
Line = tubestatusdef.ParseArgs()

# Throttling
Run = tubestatusdef.Throttle(PollIntervalMinutes,Throttling,TFileName)

# Gather the line status data
LineStatusData = tubestatusdef.RetrieveTFLData(Line,Run,SFileName)

# Generate the status output and print
if (StatusOutput == "small") and (LineStatusData == "Good Service"):
    print ("TFL " + Line[0] + Line[1] + Line[2] + " OK")
elif StatusOutput == "small":
    print ("TFL " + Line[0] + Line[1] + Line[2] + " " + LineStatusData)
else:
    print ("TFL " + Line + " line has " + LineStatusData)
