#!/usr/bin/env python2

import re
import sys
import time
import socket
import platform
import subprocess
import random
import string

# change to your carbon server ip
CARBON_SERVER = '127.0.0.1'
# change to your carbon server port
CARBON_PORT = 2104
DELAY = 10

SUCCESS_COUNT = 300
FAIL_COUNT = 50

#totally metrics number = MAX_NODE * MAX_SC * MAX_SP * MAX_APP * MAX_API * MAX_STATUS * 4(count, max, min, avg) = 640000, by default
MAX_NODE = 8 
MAX_SC = 1
MAX_SP = 1
MAX_APP = 4000
MAX_API = 5

STATUS_LIST = [200]
MAX_STATUS = len(STATUS_LIST)

APP_NAME = [[[0 for i in range(MAX_APP)] for i in range(MAX_SP)]for i in range(MAX_SC)]

def generateTotalKeyNumber():
    return MAX_NODE * MAX_SC * MAX_SP * MAX_APP * MAX_API * MAX_STATUS

def generateIndex():
    nodeIndex = random.randint(1, MAX_NODE)
    scIndex = random.randint(1, MAX_SC)
    spIndex = random.randint(1, MAX_SP)
    appIndex = random.randint(1, MAX_APP)
    apiIndex = random.randint(0, MAX_API - 1)
    status = random.choice(STATUS_LIST)
    output = [nodeIndex, scIndex, spIndex, appIndex, apiIndex, status]
    return output

def generateAppName():
    for scIndex in range(MAX_SC):
       for spIndex in range(MAX_SP):
          for appIndex in range(MAX_APP):
              APP_NAME[scIndex][spIndex][appIndex] = random_str()

def generateCommonKey(nodeIndex, scIndex, spIndex, appIndex, apiIndex, status):
    appName = APP_NAME[scIndex-1][spIndex-1][appIndex-1]
    index = [str(nodeIndex), str(scIndex), str(spIndex), str(appIndex), str(apiIndex+1), str(status)]
    keyPattern = "metrics.node-%s.api.ac.sccSc%s.sccSp%s.sccApp%s.api_v%s_0.%s"
    key = keyPattern % tuple(index)
    return key
    
def generateMetricsCount(nodeIndex, scIndex, spIndex, appIndex, apiIndex, status):
    return generateCommonKey(nodeIndex, scIndex, spIndex, appIndex, apiIndex, status) + ".count %s %d";

def generateMetricsResponseTime(nodeIndex, scIndex, spIndex, appIndex, apiIndex, status):
    key = generateCommonKey(nodeIndex, scIndex, spIndex, appIndex, apiIndex, status) + ".t"
    postFix = "%s %d"
    keyAvg = key + ".avg " + postFix
    keyMin = key + ".min " + postFix
    keyMax = key + ".max " + postFix
    output = [keyAvg, keyMin, keyMax]
    return output

def randomInRange(min, max):
    return random.randint(min, max) + min

def getResponseTimeBase(apiIndex):
    if apiIndex == 0:
         return 50
    else:
         return 1
    
def generateResponseTimeValue(apiIndex):
    base = getResponseTimeBase(apiIndex)
    avg = randomInRange(base, base + 100)
    min_ = randomInRange(0, min([avg, 50]))
    max_ = randomInRange(avg, base + 200)
    output = [avg, min_, max_]
    return output

def random_str(randomlength=8):
    return ''.join(random.sample(string.ascii_letters + string.digits, randomlength))

def run(sock, delay):
    """Make the client go go go"""
    generateAppName()
    while True:
        now = int(time.time())
        lines = []
        for nodeId in range(1, MAX_NODE + 1):
            for scId in range(1, MAX_SC + 1):
                for spId in range(1, MAX_SP + 1):
                    for appId in range(1, MAX_APP + 1):
                        for apiId in range(0, MAX_API):
                            for status in STATUS_LIST:
                                metricsCount = generateMetricsCount(nodeId, scId, spId, appId, apiId, status)
                                metricsTime = generateMetricsResponseTime(nodeId, scId, spId, appId, apiId, status)
                                timeValue = generateResponseTimeValue(apiId)
                                lines.append(metricsCount % (1, now))
                                lines.append(metricsTime[0] % (timeValue[0], now))
                                lines.append(metricsTime[1] % (timeValue[1], now))
                                lines.append(metricsTime[2] % (timeValue[2], now))

        message = '\n'.join(lines) + '\n'
        print "sending metrics count: " + str(len(lines))    
        sock.sendall(message)
        print 'sent.' + '-' * 80
        time.sleep(delay)

def main():
    """Wrap it all up together"""
    delay = DELAY
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg.isdigit():
            delay = int(arg)
        else:
            sys.stderr.write("Ignoring non-integer argument. Using default: %ss\n" % delay)

    sock = socket.socket()
    try:
        sock.connect( (CARBON_SERVER, CARBON_PORT) )
    except socket.error:
        raise SystemExit("Couldn't connect to %(server)s on port %(port)d, is carbon-cache.py running?" % { 'server':CARBON_SERVER, 'port':CARBON_PORT })

    try:
        run(sock, delay)
    except KeyboardInterrupt:
        sys.stderr.write("\nExiting on CTRL-c\n")
        sys.exit(0)

if __name__ == "__main__":
    main()
