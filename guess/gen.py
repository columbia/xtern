#!/usr/bin/env python

import coloroutput 
import logging
import argparse
import os
import re
import sys

def forEachSubDir(dir):
    cost = {}
    reobj = re.compile('output.[0-9]+')
    os.chdir(s_d)
    for root, dirs, files  in os.walk('.'):
        for f in files:
            if reobj.match(f):
                log_number = f.split(".")[-1]
                log_file = '%s/%s' % (root, f)
                for line in reversed(open(log_file, 'r').readlines()):
                    if re.search('^real [0-9]+\.[0-9][0-9][0-9]$', line):
                        cost[int(log_number)] = [float(line.split()[1])]
    del cost[0]
    values = tuple(cost[key] for key in cost)
    os.chdir('..')
    return cost

def genStats(values):
    try:
        import numpy as np
    except ImportError as e:
        logging.error("please install 'numpy' module. %s" % e)
        sys.exit(1)
    try:
        import scipy
    except ImportError as e:
        logging.error("please install 'scipy' module. %s" % e)
        sys.exit(1)
    avg = np.average(np)
    

if __name__ == "__main__":
    # set log format
    logger = logging.getLogger()
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s",
                                  "%Y%b%d-%H:%M:%S")
    ch = logging.StreamHandler()
    ch.setFormatter(formatter)
    ch.setLevel(logging.DEBUG)
    logger.addHandler(ch)
    logger.setLevel(logging.DEBUG)

    # parse input arguments
    parser = argparse.ArgumentParser(
        description="Generate perforamnce report of Parrot benchmarks")
    parser.add_argument('directory', nargs='+',
        type=str,
        help = "list of directories")
    args = parser.parse_args()

    for d in args.directory:
        os.chdir(d)
        for s_d in os.walk('.').next()[1]:
            values = forEachSubDir(s_d)
            stats_str = genStats(values)
        os.chdir('..')
