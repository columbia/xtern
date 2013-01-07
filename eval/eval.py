#!/usr/bin/env python

import ConfigParser
import argparse
import os
import sys
import errno
import logging
import coloroutput
import re
import subprocess

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
    except IOError as e:
        logging.error("There is no 'default.options' file")
        sys.exit(1)
    logging.debug("default.options : ")
    for key in default:
        logging.debug("\t{0} = '{1}'".format(key,default[key]))
    return default
    
def getConfigFullPath(config_file):
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
        newConfig = ConfigParser.ConfigParser( {"REPEATS": "100",
                                       "INPUTS": ""} )
        ret = newConfig.read(config_file)
    except ConfigParser.MissingSectionHeaderError as e:
        logging.error(str(e))
    except ConfigParser.ParsingError as e:
        logging.error(str(e))
    except ConfigParser.Error as e:
        logging.critical("strange error? " + str(e))
    else:
        if ret:
            return newConfig

def getGitInfo():
    import commands
    git_show = 'cd '+XTERN_ROOT+' && git show '
    githash = commands.getoutput(git_show+'| head -1 | sed -e "s/commit //"')
    git_diff = 'cd '+XTERN_ROOT+' && git diff --quiet'
    gitstatus = commands.getoutput('if ! ('+
                                   git_diff+
                                   '); then echo "_dirty"; fi')
    commit_date = commands.getoutput( git_show+
            '| head -4 | grep "Date:" | sed -e "s/Date:[ \t]*//"' )
    date_tz  = re.compile(r'^.* ([+-]\d\d\d\d)$').match(commit_date).group(1)
    date_fmt = ('%%a %%b %%d %%H:%%M:%%S %%Y %s') % date_tz
    import datetime
    gitcommitdate = str(datetime.datetime.strptime(commit_date, date_fmt))
    logging.debug( "git 6 digits hash code: " + githash[0:6] )
    logging.debug( "git reposotory status: " + gitstatus)
    logging.debug( "git commit date: ")
    return [githash[0:6], gitstatus, gitcommitdate]

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # for newer python version; can specify flags
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            logging.warning("%s already exists" % path)
            pass
        else: raise

def genRunDir(config_file, git_info):
    from os.path import basename
    config_name = os.path.splitext( basename(config_file) )[0]
    from time import strftime
    dir_name = config_name + strftime("%Y%b%d_%H%M%S") + '_' + git_info[0] + git_info[1]
    mkdir_p(dir_name)
    logging.debug("creating %s" % dir_name)
    return os.path.abspath(dir_name)

def extract_apps_exec(bench):
    bench = bench.partition('"')[0].partition("'")[0]
    apps = bench.split()
    if apps.__len__() < 1:
        raise Exception("cannot parse executible file name")
    elif apps.__len__() == 1:
        return apps[0], os.path.abspath(APPS + '/' + apps[0] + '/' + apps[0])
    else:
        return apps[0], os.path.abspath(APPS + '/' + apps[0] + '/' + apps[1])

def generate_local_options(config, bench):
    config_options = config.options(bench)
    output = ""
    for option in default_options:
        if option in config_options:
            entry = option + ' = ' + config.get(bench, option)
            logging.info(entry)
        else:
            entry = option + ' = ' + default_options[option]
            #logging.debug(entry)
        output += '%s\n' % entry
    with open("local.options", "w") as option_file:
        option_file.write(output)

def processBench(config, bench):
    logging.debug("processing: " + bench)
    apps_name, exec_file = extract_apps_exec(bench)
    logging.debug("app = %s" % apps_name)
    logging.debug("executible file = %s" % exec_file)
    segs = re.sub(r'(\")|(\.)|/|\'', '', bench).split()
    dir_name =  '_'.join(segs)
    
    mkdir_p(dir_name)
    os.chdir(dir_name)

    # generate local options
    generate_local_options(config, bench)
    inputs = config.get(bench, 'inputs')
    repeats = config.get(bench, 'repeats')
    
    xtern_env = os.environ.copy()
    xtern_env['LD_PRELOAD'] = "%s/dync_hook/interpose.so" % XTERN_ROOT

    for i in range(int(repeats)):
        proc = subprocess.Popen(['time', '-p', exec_file] + inputs.split(), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=xtern_env)
        proc.wait()
        os.system('mv out out.%d' % i)
        with open('output.%d' % i, 'w') as log_file:
            log_file.write(proc.stdout.read())

    # FIXME: performance issue
    cost = []
    for i in range(int(repeats)):
        for line in open('output.%d' % i, 'r'):
            if re.search('^real ', line):
                cost += [float(line.split()[1])]
    logging.info('Average Cost: %f' % (sum(cost)/float(repeats)))


    os.chdir("..")

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
    APPS = os.path.abspath(XTERN_ROOT + "/apps/")
    XTERN_PRELOAD = "LD_PRELOAD=%s/dync_hook/interpose.so" % XTERN_ROOT
    RAND_PRELOAD = "LDPRELOAD=%s/eval/rand-intercept/rand-intercept.so"

    # get default xtern options
    default_options = getXternDefaultOptions()
    git_info = getGitInfo()
    root_dir = os.getcwd()

    for config_file in args.filename:
        logging.info("processing '" + config_file + "'")
        # get absolute path of config file
        full_path = getConfigFullPath(config_file)

        # config parser  
        local_config = readConfigFile(full_path)
        if not local_config:
            logging.warning("skip " + full_path)
            continue

        # generate running directory
        run_dir = genRunDir(full_path, git_info)
        if not run_dir:
            continue
        os.chdir(run_dir)
        
        benchmarks = local_config.sections()
        for benchmark in benchmarks:
            if benchmark == "default" or benchmark == "example":
                continue
            processBench(local_config, benchmark)

        os.chdir(root_dir)
       
