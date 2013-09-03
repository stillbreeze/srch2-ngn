#this test is used for exact A1
#using: python exact_A1.py queriesAndResults.txt

import sys, urllib2, json, time, subprocess, os, commands, signal

port = '8081'

#make sure that start the engine up
def pingServer():
    info = 'curl -s http://localhost:' + port + '/search?q=Garden | grep -q results'
    while os.system(info) != 0:
        time.sleep(1)
        info = 'curl -s http://localhost:' + port + '/search?q=Garden | grep -q results'

def testSaveShutdownRestart(binary_path):
    #Start the engine server
    binary= binary_path + '/srch2-search-server'
    binary= binary+' --config-file=./exact_a1/conf.ini &'
    print 'starting engine: ' + binary
    os.popen(binary)

    pingServer()

    #save the index
    shutdownCommand='curl -s http://localhost:' + port + '/save -X PUT'
    os.system(shutdownCommand)

    
    #shutdown
    shutdownCommand='curl -s http://localhost:' + port + '/shutdown -X PUT'
    os.system(shutdownCommand)

    #search a query for checking if the server is shutdown
    try:
        query='http://localhost:' + port + '/search?q=good'
        response = urllib2.urlopen(query).read()
        print response
    except:
        print 'server has been shutdown'
    else:
        print 'server is not shutdown'
        return 1
    #restart
    os.popen(binary)
    pingServer()
    #search a query for checking if the server is shutdown
    query='http://localhost:' + port + '/search?q=good'
    response = urllib2.urlopen(query).read()
    if response == 0:
        print 'server does not start'
        return 1
    else:
        print 'server start'
    #get pid of srch2-search-server and kill the process
    s = commands.getoutput('ps aux | grep srch2-search-server')
    stat = s.split() 
    os.kill(int(stat[1]), signal.SIGUSR1)
    print 'test pass'
    print '=============================='

if __name__ == '__main__':      
    #Path of the query file
    #each line like "trust||01c90b4effb2353742080000" ---- query||record_ids(results)
    binary_path = sys.argv[1]
    testSaveShutdownRestart(binary_path)
