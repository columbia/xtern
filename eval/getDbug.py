#!/usr/bin/env python

import os
import argparse
import sys

def checkExist(file, flags=os.F_OK):
    if not os.path.exists(file) or not os.access(file, flags):
        return False
    return True

def getResult(directory, app):
    file = directory + '/' + app
    for line in reversed(open(file, 'r').readlines()):
        if 'Estimated number of iterations' in line:
            print '================================='
            print directory[directory.find('stl_')+4:]
            print line[line.find(':')+1:]
            print '================================='
            break

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Collect dbug performance number for excel')
    parser.add_argument('dirname',
        type = str,
        help = 'directory containing the dbug result (e.g. current)') 
    args = parser.parse_args()
    if args.dirname.__len__() == 0:
        print 'Please specify a directory!'
        sys.exit(1)        
    else:
        directory = os.path.abspath(args.dirname)
        if not checkExist(directory):
            print 'Invalid directory name: ' + directory
        else:
            for name in os.listdir(directory):
                if name == "git_diff":
                    continue
                temp_name = 'dbug.log'
                getResult(os.path.abspath(directory+'/'+name), temp_name) 
