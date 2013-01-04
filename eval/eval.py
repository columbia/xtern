#!/usr/bin/env python

import ConfigParser
import argparse
import os
import sys
import errno
import logging
import coloroutput
import re

def getXternDefaultOptions():
    try:
        with open(XTERN_ROOT+'/default.options') as f:
            for line in f:
                line = line.partition('#')[0].rstrip()
                if not line:
                    continue
                #print line
                #if re.search('(?#...)', line) is None:
                #    sys.stdout.write(line)
    except IOError as e:
        logging.error("There is no 'default.options' file")
        sys.exit(1)
    

if __name__ == "__main__":
    # setting log format
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

    # parse input arguments
    parser = argparse.ArgumentParser(
        description="Evaluate the perforamnce of xtern")
    parser.add_argument('filename', nargs='*',
        type=str,
        default = ["xtern.cfg"],
        help = "list of configuration files (default: xtern.cfg)")
    args = parser.parse_args()

    if args.filename.__len__() == 0:
        logging.critical('no configuration file specified??')
        sys.exit(1)
    elif args.filename.__len__() == 1:
        logging.debug('config file: ' + ''.join(args.filename))
    else:
        logging.debug('config files: ' + ', '.join(args.filename))
    
    # get environment variable
    try:
        XTERN_ROOT = os.environ["XTERN_ROOT"]
        logging.debug('XTERN_ROOT = ' + XTERN_ROOT)
    except KeyError as e:
        logging.error("Please set the environment variable " + str(e))
        sys.exit(1)

    # get default xtern options
    getXternDefaultOptions()

    # process the config files
    for config_file in args.filename:
        logging.info("processing '" + config_file + "'")
        
    #print os.getcwd()
    #config = ConfigParser.ConfigParser()
