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
import time
import signal
import threading
from evalutils import mkDirP, copyFile, which, checkExist

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
    #logging.debug("default.options : ")
    #for key in default:
    #    logging.debug("\t{0} = '{1}'".format(key,default[key]))
    return default

def getXternOverwriteOptions():
    overwrite = {}
    if args.options:
        try:
            with open(args.options) as f:
                for line in f:
                    line = line.partition('#')[0].rstrip()
                    if not line:
                        continue
                    s1, s2 = [p.strip() for p in line.split("=")]
                    if not s1 or not s2:
                        loggin.warning(
                            'cannot get value from "%s" (ignored)' % line)
                        continue
                    overwrite[s1] = s2
        except IOError as e:
            logging.error(e)
            sys.exit(1)
    return overwrite
    
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
                                                "INPUTS": "",
                                                "REQUIRED_FILES": "",
                                                "DOWNLOAD_FILES": "",
                                                "TARBALL": "",
                                                "GZIP": "",
                                                "EXPORT": "",
                                                "INIT_ENV_CMD": "",
                                                "EVALUATION": ""} )
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

# TODO: will fail if there is no git information to get
def getGitInfo():
    import commands
    git_show = 'cd '+XTERN_ROOT+' && git show '
    githash = commands.getoutput(git_show+'| head -1 | sed -e "s/commit //"')
    git_diff = 'cd '+XTERN_ROOT+' && git diff --quiet'
    diff = commands.getoutput('cd ' +XTERN_ROOT+ ' && git diff')
    if diff:
        gitstatus = '_dirty'
    else:
        gitstatus = ''
    commit_date = commands.getoutput( git_show+
            '| head -4 | grep "Date:" | sed -e "s/Date:[ \t]*//"' )
    date_tz  = re.compile(r'^.* ([+-]\d\d\d\d)$').match(commit_date).group(1)
    date_fmt = ('%%a %%b %%d %%H:%%M:%%S %%Y %s') % date_tz
    import datetime
    gitcommitdate = str(datetime.datetime.strptime(commit_date, date_fmt))
    logging.debug( "git 6 digits hash code: " + githash[0:6] )
    logging.debug( "git reposotory status: " + gitstatus)
    logging.debug( "git commit date: " + gitcommitdate)
    return [githash[0:6], gitstatus, gitcommitdate, diff]

def genRunDir(config_file, git_info):
    from os.path import basename
    config_name = os.path.splitext( basename(config_file) )[0]
    if args.no_parrot:
        dir_name = "N"
    else:
        dir_name = ""
    from time import strftime
    dir_name += config_name + strftime("%Y%b%d_%H%M%S") + '_'  + git_info[0] + git_info[1]
    mkDirP(dir_name)
    logging.debug("creating %s" % dir_name)
    return os.path.abspath(dir_name)

def extractAppsExec(bench, apps_dir=""):
    bench = bench.partition('"')[0].partition("'")[0]
    apps = bench.split()
    if apps.__len__() < 1:
        raise Exception("cannot parse executible file name")
    elif apps.__len__() == 1:
        return apps[0], os.path.abspath('%s/%s/%s' %(apps_dir, apps[0], apps[0]))
    else:
        return apps[0], os.path.abspath('%s/%s/%s' %(apps_dir, apps[0], apps[1]))

def generateLocalOptions(config, bench):
    config_options = config.options(bench)
    output = ""
    for option in default_options:
        if option in overwrite_options:
            entry = option + ' = ' + overwrite_options[option]
            logging.debug(entry)
        elif option in config_options:
            entry = option + ' = ' + config.get(bench, option)
            logging.debug(entry)
        else:
            entry = option + ' = ' + default_options[option]
            #logging.debug(entry)
        output += '%s\n' % entry
    with open("local.options", "w") as option_file:
        option_file.write(output)

def copyRequiredFiles(app, files):
    for f in files.split():
        logging.debug("copying required file : %s" % f)
        dst = os.path.basename(f)
        if os.path.isabs(f):
            src = f
        else:
            src = os.path.abspath('%s/apps/%s/%s' % (XTERN_ROOT, app, f))
        try:
            copyFile(os.path.realpath(src), dst)
        except IOError as e:
            logging.warning(str(e))
            return False
    return True

def downloadFilesFromWeb(links):
    import urllib
    opener = urllib.URLopener()
    for link in links.split():
        logging.debug("Downloading file from %s" % link)
        try:
            opener.retrieve(link, link.split('/')[-1])
        except IOError as e:
            logging.warning(str(e))
            return False
    return True

def extractTarBall(app, files):
    for f in files.split():
        logging.debug("extracting file : %s" % f)
        if os.path.isabs(f):
            src = f
        else:
            src = os.path.abspath('%s/apps/%s/%s' % (XTERN_ROOT, app, f))
        
        import tarfile
        try:
            tarfile.is_tarfile(src)
            with tarfile.open(src, 'r') as t:
                t.extractall()
        except IOError as e:
            logging.warning(str(e))
            return False
        except tarfile.TarError as e:
            logging.warning(str(e))
            return False
    return True

def extractGZip(app, files):
    for f in files.split():
        logging.debug("extracting gzip file : %s" % f)
        if os.path.isabs(f):
            src = f
        else:
            src = os.path.abspath('%s/apps/%s/%s' % (XTERN_ROOT, app, f))
        
        import tarfile
        try:
            tarfile.is_tarfile(src)
            with tarfile.open(src, 'r:gz') as t:
                t.extractall()
        except IOError as e:
            logging.warning(str(e))
            return False
        except tarfile.TarError as e:
            logging.warning(str(e))
            return False
    return True

def preSetting(config, bench, apps_name):
    # copy required files
    required_files = config.get(bench, 'required_files')
    if not copyRequiredFiles(apps_name, required_files):
        logging.warning("error in config [%s], skip" % bench)
        return

    # download files if needed
    download_files = config.get(bench, 'download_files')
    if not downloadFilesFromWeb(download_files):
        logging.warning("cannot download one of files in config [%s], skip" % bench)
        return

    # extract *.tar files
    tar_balls = config.get(bench, 'tarball')
    if not extractTarBall(apps_name, tar_balls):
        logging.warning("cannot extract files in config [%s], skip" % bench)
        return

    gzips = config.get(bench, 'gzip')
    if not extractGZip(apps_name, gzips):
        logging.warning("cannot extract files in config [%s], skip" % bench)
        return
    
def execBench(cmd, repeats, out_dir, init_env_cmd=""):
    mkDirP(out_dir)
    for i in range(int(repeats)+1):
        sys.stderr.write("        PROGRESS: %5d/%d\r" % (i, int(repeats))) # progress
        with open('%s/output.%d' % (out_dir, i), 'w', 102400) as log_file:
            if init_env_cmd:
                os.system(init_env_cmd)
            #proc = subprocess.Popen(xtern_command, stdout=sys.stdout, stderr=sys.stdout,
            proc = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT,
                                    shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
            try: # TODO should handle whole block
                proc.wait()
            except KeyboardInterrupt as k:
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                except:
                    pass
                raise k

        # move log files into 'xtern' directory
        try:
            os.renames('out', '%s/out.%d' % (out_dir, i))
        except OSError:
            pass

def processBench(config, bench):
    # for each bench, generate running directory
    logging.debug("processing: " + bench)
    apps_name, exec_file = extractAppsExec(bench, XTERN_APPS)
    logging.debug("app = %s" % apps_name)
    logging.debug("executible file = %s" % exec_file)
    if not checkExist(exec_file, os.X_OK):
        logging.warning('%s does not exist, skip [%s]' % (exec_file, bench))
        return

    segs = re.sub(r'(\")|(\.)|/|\'', '', bench).split()
    dir_name = ""
    dir_name =  '_'.join(segs)
    mkDirP(dir_name)
    os.chdir(dir_name)

    # get variables
    generateLocalOptions(config, bench)
    inputs = config.get(bench, 'inputs')
    repeats = config.get(bench, 'repeats')
    export = config.get(bench, 'EXPORT')
    if export:
        logging.debug("export %s", export)

    # get required files
    preSetting(config, bench, apps_name)

    # git environment presetting command
    init_env_cmd = config.get(bench, 'INIT_ENV_CMD')
    if init_env_cmd:
        logging.info("presetting cmd in each round: %s" % init_env_cmd)

    # generate commands
    if not args.no_parrot:
        xtern_command = ' '.join(['time', XTERN_PRELOAD, export, exec_file] + inputs.split())
        logging.info("executing '%s'" % xtern_command)
        execBench(xtern_command, repeats, 'xtern', init_env_cmd)
    else:
        nondet_command = ' '.join(['time', export, exec_file] + inputs.split())
        logging.info("executing '%s'" % nondet_command)
        execBench(nondet_command, repeats, 'non-det', init_env_cmd)

    # copy exec file
    copyFile(os.path.realpath(exec_file), os.path.basename(exec_file))

    os.chdir("..")

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
        description="Evaluate the perforamnce of Parrot")
    parser.add_argument('filename', nargs='*',
        type=str,
        default = ["parrot.cfg"],
        help = "list of configuration files (default: parrot.cfg)")
    parser.add_argument("--no-parrot",
                        action="store_true",
                        help="run benchmark without Parrot")
    parser.add_argument("--options",
                        nargs='?',
                        default=None,
                        metavar="option.txt",
                        help="file contains the global options which overwrites those specified in *.cfg")
    args = parser.parse_args()

    if args.filename.__len__() == 0:
        logging.critical('no configuration file specified??')
        sys.exit(1)
    elif args.filename.__len__() == 1:
        logging.debug('config file: ' + ''.join(args.filename))
    else:
        logging.debug('config files: ' + ', '.join(args.filename))

    # get/set environment variable
    try:
        XTERN_ROOT = os.environ["XTERN_ROOT"]
        logging.debug('XTERN_ROOT = ' + XTERN_ROOT)
    except KeyError as e:
        logging.error("Please set the environment variable " + str(e))
        sys.exit(1)
    XTERN_APPS = os.path.abspath(XTERN_ROOT + "/apps/")
    logging.debug("set timeformat to '\\nreal %E\\nuser %U\\nsys %S'")
    os.environ['TIMEFORMAT'] = "\nreal %E\nuser %U\nsys %S"
 
    # check xtern files
    if not checkExist("%s/dync_hook/interpose.so" % XTERN_ROOT, os.R_OK):
        logging.error('thre is no "$XTERN_ROOT/dync_hook/interpose.so"')
        sys.exit(1)
    XTERN_PRELOAD = "LD_PRELOAD=%s/dync_hook/interpose.so" % XTERN_ROOT

    # find 'bash'
    bash_path = which('bash')
    if not bash_path:
        logging.critical("cannot find shell 'bash'")
        sys.exit(1)
    else:
        bash_path = bash_path[0]
        logging.debug("find 'bash' at %s" % bash_path)

    # get default xtern options
    default_options = getXternDefaultOptions()
    overwrite_options = getXternOverwriteOptions()
    git_info = getGitInfo()
    root_dir = os.getcwd()

    # loop over each *.cfg
    for config_file in args.filename:
        logging.info("processing '" + config_file + "'")

        # config parser  
        config_full_path = getConfigFullPath(config_file)
        local_config = readConfigFile(config_full_path)
        if not local_config:
            logging.warning("skip " + config_full_path)
            continue

        # generate running directory
        run_dir = genRunDir(config_full_path, git_info)
        if not run_dir:
            continue
        try:
            os.unlink('current')
        except OSError:
            pass
        os.symlink(run_dir, 'current')
        os.chdir(run_dir)

        # write diff file if the repository is dirty
        if git_info[3]:
            with open("git_diff", "w") as diff:
                diff.write(git_info[3])
        
        for benchmark in local_config.sections():
            if benchmark == "default" or benchmark == "example":
                continue
            processBench(local_config, benchmark)

        os.chdir(root_dir)
       
