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
    return values

def genStats(values):
    try:
        import numpy as np
    except ImportError as e:
        logging.error("please install 'numpy' module. %s" % e)
        sys.exit(1)
    try:
        from scipy.stats import sem
    except ImportError as e:
        logging.error("please install 'scipy' module. %s" % e)
        sys.exit(1)
    avg = np.average(values)
    std_error = sem(values, axis=None)
    ret = '{:<8.4f} {:<13.10f} {:.2%}'.format(avg, std_error, std_error/avg)
    return ret
    
def extract(dir):
    eval_id = dir.split('_')[0]
    name = dir.split('_')[2]
    # handle special names
    if name == 'water' or name == 'lu' or name == 'ocean':
        name = "_".join([name, dir.split('_')[3]])
    return eval_id, '{:<25}'.format(name)

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

    result = {}
    for d in args.directory:
        os.chdir(d)
        for s_d in os.walk('.').next()[1]:
            values = forEachSubDir(s_d)
            stats_str = genStats(values)
            eval_id, bench_name = extract(s_d)
            result[int(eval_id)] = " ".join([bench_name, stats_str])
        os.chdir('..')
        
        if d.endswith('.dir'):
            output = os.path.abspath(d).rstrip('.dir')
        else:
            output = "%s.stat" % os.path.abspath(d)
        with open(output, 'w') as f:
            f.write("#ID  {:<25} {:<8} {:<13} {:<4} {:<10}\n".format(
                'Name', 'Mean', 'SEM', 'ERR(%)', 'STD'))
            for key in result:
                f.write("%4s %s\n" % (key,result[key]) )

