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
    default = {}
    try:
        with open(XTERN_ROOT+'/default.options') as f:
            for line in f:
                line = line.partition('#')[0].rstrip()
                if not line:
                    continue
                s1, s2 = [p.strip() for p in line.split("=")]
                if not s1 or not s2:
                    logging.warning(
                        'cannot get default value from "%s" (ignored)' % line)
                    continue
                default[s1] = s2
                #print ', '.join(line_segs)
                #print line
                #if re.search('(?#...)', line) is None:
                #    sys.stdout.write(line)
    except IOError as e:
        logging.error("There is no 'default.options' file")
        sys.exit(1)
    logging.debug("default.options : ")
    for key in default:
        logging.debug("\t{0} = '{1}'".format(key,default[key]))
    return default
    
def getFullPath(config_file):
    try:
        with open(config_file) as f: pass
    except IOError as e:
        if config_file == 'xtern.cfg':
            logging.warning("'xtern.cfg' does not exist in current directory"
                    + ", use default one in XTERN_ROOT/eval")
            return os.path.abspath(XTERN_ROOT + "/eval/xtern.cfg")
        else:
            logging.warning("'%s' does not exist" % config_file)
            return None
    return os.path.abspath(config_file)

def readConfigFile(config_file):
    try:
        ret = config.read(config_file)
    except ConfigParser.MissingSectionHeaderError as e:
        logging.error(str(e))
    except ConfigParser.ParsingError as e:
        logging.error(str(e))
    except ConfigParser.Error as e:
        logging.critical("strange error? " + str(e))
    else:
        if ret:
            return config


if __name__ == "__main__":
    # setting log format
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
    default_options = getXternDefaultOptions()
    # config parser
    config = ConfigParser.ConfigParser( {"REPEATS":100,
                                       "INPUTS": ""} )

    for config_file in args.filename:
        logging.info("processing '" + config_file + "'")
        # get absolute path of config file
        full_path = getFullPath(config_file)

        if not readConfigFile(full_path):
            logging.warning("skip " + full_path)
            continue

        # generate running directory
        
        
    #print os.getcwd()
