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
                                                "DTHREADS": "",
                                                "INIT_ENV_CMD": "",
                                                "C_WITH_XTERN": "0",
                                                "C_CMD": "",
                                                "C_TERMINATE_SERVER": "0",
                                                "C_STATS": "0",
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

def checkExist(file, flags=os.X_OK):
    if not os.path.exists(file) or not os.path.isfile(file) or not os.access(file, flags):
        return False
    return True

def copy_file(src, dst):
    import shutil
    shutil.copy(src, dst)

# ref: twisted-12.3.0 procutils.py
def which(name, flags=os.X_OK):
    result = []
    path = os.environ.get('PATH', None)
    if path is None:
        return []
    for p in os.environ.get('PATH', '').split(os.pathsep):
        p = os.path.join(p, name)
        if os.access(p, flags):
            result.append(p)
    return result

def write_stats(xtern, nondet, repeats):
    try:
        import numpy
    except ImportError:
        logging.error("please install 'numpy' module")
        sys.exit(1)
    xtern_avg = numpy.average(xtern)
    xtern_std = numpy.std(xtern)
    nondet_avg = numpy.average(nondet)
    nondet_std = numpy.std(nondet)
    overhead_avg = xtern_avg/nondet_avg - 1.0
    import math
    #overhead_std = math.fabs(overhead_avg)*(math.sqrt(((nondet_std/nondet_avg)**2) + (xtern_std/xtern_avg)**2))
    with open("stats.txt", "w") as stats:
        stats.write('overhead: {1:.3f}%\n\tavg {0}\n'.format(overhead_avg, overhead_avg*100))
        stats.write('xtern:\n\tavg {0}\n\tsem {1}\n'.format(xtern_avg, xtern_std/math.sqrt(repeats)))
        stats.write('non-det:\n\tavg {0}\n\tsem {1}\n'.format(nondet_avg, nondet_std/math.sqrt(repeats)))

def write_stats_dthread(dthread, nondet, repeats):
    try:
        import numpy
    except ImportError:
        logging.error("please install 'numpy' module")
        sys.exit(1)
    dthread_avg = numpy.average(dthread)
    dthread_std = numpy.std(dthread)
    nondet_avg = numpy.average(nondet)
    nondet_std = numpy.std(nondet)
    overhead_avg = dthread_avg/nondet_avg - 1.0
    import math
    #overhead_std = math.fabs(overhead_avg)*(math.sqrt(((nondet_std/nondet_avg)**2) + (dthread_std/dthread_avg)**2))
    with open("stats.txt", "a") as stats:
        stats.write('dthread-overhead: {1:.3f}%\n\tavg {0}\n'.format(overhead_avg, overhead_avg*100))
        stats.write('dthread:\n\tavg {0}\n\tsem {1}\n'.format(dthread_avg, dthread_std/math.sqrt(repeats)))


def copy_required_files(app, files):
    for f in files.split():
        logging.debug("copying required file : %s" % f)
        dst = os.path.basename(f)
        if os.path.isabs(f):
            src = f
        else:
            src = os.path.abspath('%s/apps/%s/%s' % (XTERN_ROOT, app, f))
        try:
            copy_file(os.path.realpath(src), dst)
        except IOError as e:
            logging.warning(str(e))
            return False
    return True

def download_files_from_web(links):
    #from urllib import urlretrieve
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

def extract_tarball(app, files):
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

def extract_gzip(app, files):
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
    if not copy_required_files(apps_name, required_files):
        logging.warning("error in config [%s], skip" % bench)
        return

    # download files if needed
    download_files = config.get(bench, 'download_files')
    if not download_files_from_web(download_files):
        logging.warning("cannot download one of files in config [%s], skip" % bench)
        return

    # extract *.tar files
    tar_balls = config.get(bench, 'tarball')
    if not extract_tarball(apps_name, tar_balls):
        logging.warning("cannot extract files in config [%s], skip" % bench)
        return

    gzips = config.get(bench, 'gzip')
    if not extract_gzip(apps_name, gzips):
        logging.warning("cannot extract files in config [%s], skip" % bench)
        return
    
 
def execBench(cmd, repeats, out_dir,
              client_cmd="", client_terminate_server=False,
              init_env_cmd=""):
    mkdir_p(out_dir)
    for i in range(int(repeats)):
        sys.stderr.write("\tPROGRESS: %5d/%d\r" % (i+1, int(repeats))) # progress
        with open('%s/output.%d' % (out_dir, i), 'w', 102400) as log_file:
            if init_env_cmd:
                os.system(init_env_cmd)
            #proc = subprocess.Popen(xtern_command, stdout=sys.stdout, stderr=sys.stdout,
            proc = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT,
                                    shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
            if client_cmd:
                time.sleep(1)
                with open('%s/client.%d' % (out_dir, i), 'w', 102400) as client_log_file:
                    client_proc = subprocess.Popen(client_cmd, stdout=client_log_file, stderr=subprocess.STDOUT,
                                                   shell=True, executable=bash_path, bufsize = 102400)
                    client_proc.wait()
                if client_terminate_server:
                    os.killpg(proc.pid, signal.SIGTERM)
                proc.wait()
                time.sleep(2)
            else:
                proc.wait()
        # move log files into 'xtern' directory
        try:
            os.renames('out', '%s/out.%d' % (out_dir, i))
        except OSError:
            pass

def processBench(config, bench):
    # for each bench, generate running directory
    logging.debug("processing: " + bench)
    specified_evaluation = config.get(bench, 'EVALUATION')
    apps_name, exec_file = extract_apps_exec(bench)
    logging.debug("app = %s" % apps_name)
    logging.debug("executible file = %s" % exec_file)
    if not specified_evaluation and not checkExist(exec_file, os.X_OK):
        logging.warning('%s does not exist, skip [%s]' % (exec_file, bench))
        return

    segs = re.sub(r'(\")|(\.)|/|\'', '', bench).split()
    dir_name =  '_'.join(segs)
    mkdir_p(dir_name)
    os.chdir(dir_name)

    # generate local options
    generate_local_options(config, bench)
    inputs = config.get(bench, 'inputs')
    repeats = config.get(bench, 'repeats')

    # if specified evaluation script; use it
    if specified_evaluation:
        specified = __import__(specified_evaluation, globals(), locals(), [], -1)
        specified.evaluation(int(repeats))
        return

    # get required files
    preSetting(config, bench, apps_name)

    # get 'export' environment variable
    export = config.get(bench, 'EXPORT')
    if export:
        logging.debug("export %s", export)

    # git environment presetting command
    init_env_cmd = config.get(bench, 'INIT_ENV_CMD')
    if init_env_cmd:
        logging.info("presetting cmd in each round %s" % init_env_cmd)

    # check if this is a server-client app
    client_cmd = config.get(bench, 'C_CMD')
    client_terminate_server = bool(int(config.get(bench, 'C_TERMINATE_SERVER')))
    client_with_xtern = bool(int(config.get(bench, 'C_WITH_XTERN')))
    use_client_stats = bool(int(config.get(bench, 'C_STATS')))
    if client_cmd:
        if client_with_xtern:
            client_cmd = XTERN_PRELOAD + ' ' + client_cmd
        client_cmd = 'time ' + client_cmd
        logging.info("client command : %s" % client_cmd)
        logging.debug("terminate server after client finish job : " + str(client_terminate_server))
        logging.debug("evaluate performance by using stats of client : " + str(use_client_stats))

    # generate command for xtern [time LD_PRELOAD=... exec args...]
    if client_cmd and client_with_xtern:
        xtern_command = ' '.join(['time', export, exec_file] + inputs.split())
    else:
        xtern_command = ' '.join(['time', XTERN_PRELOAD, export, exec_file] + inputs.split())
    logging.info("executing '%s'" % xtern_command)
    execBench(xtern_command, repeats, 'xtern', client_cmd, client_terminate_server, init_env_cmd)

    client_cmd = config.get(bench, 'C_CMD')
    if client_cmd:
        if client_with_xtern:
            client_cmd = RAND_PRELOAD + ' ' + client_cmd
        client_cmd = 'time ' + client_cmd
    # generate command for non-det [time LD_PRELOAD=... exec args...]
    if client_cmd and client_with_xtern:
        nondet_command = ' '.join(['time', export, exec_file] + inputs.split())
    else:
        nondet_command = ' '.join(['time', RAND_PRELOAD, export, exec_file] + inputs.split())
    logging.info("executing '%s'" % nondet_command)
    execBench(nondet_command, repeats, 'non-det', client_cmd, client_terminate_server, init_env_cmd)

    # run additional benchmark for dthreads
    dthread = config.get(bench, 'DTHREADS')
    if dthread:
        if client_cmd:
            logging.error("client-server with dthreads has not yet tested...")
            sys.exit(1)
        dthread_exec_file = os.path.abspath('%s/%s/%s' % (APPS, apps_name, dthread))
        dthread_command = ' '.join(['time', export, dthread_exec_file] + inputs.split())
        logging.info("executing '%s'" % dthread_command)
        execBench(dthread_command, repeats, 'dthreads')

    # get stats
    xtern_cost = []
    for i in range(int(repeats)):
        if client_cmd and use_client_stats:
            log_file_name = 'xtern/client.%d' % i
        else:
            log_file_name = 'xtern/output.%d' % i
        for line in reversed(open(log_file_name, 'r').readlines()):
            if re.search('^real [0-9]+\.[0-9][0-9][0-9]$', line):
                xtern_cost += [float(line.split()[1])]
                break

    nondet_cost = []
    for i in range(int(repeats)):
        if client_cmd and use_client_stats:
            log_file_name = 'non-det/client.%d' % i
        else:
            log_file_name = 'non-det/output.%d' % i
        for line in reversed(open(log_file_name, 'r').readlines()):
            if re.search('^real [0-9]+\.[0-9][0-9][0-9]$', line):
                nondet_cost += [float(line.split()[1])]
                break

    dthread_cost = []
    if dthread:
        for i in range(int(repeats)):
            if client_cmd and use_client_stats:
                log_file_name = 'dthreads/client.%d' % i
            else:
                log_file_name = 'dthreads/output.%d' % i
            for line in reversed(open(log_file_name, 'r').readlines()):
                if re.search('^real [0-9]+\.[0-9][0-9]$', line):
                    dthread_cost += [float(line.split()[1])]
                    break

    write_stats(xtern_cost, nondet_cost, int(repeats))
    if dthread:
        write_stats_dthread(dthread_cost, nondet_cost, int(repeats))

    # copy exec file
    copy_file(os.path.realpath(exec_file), os.path.basename(exec_file))

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
    if not checkExist("%s/dync_hook/interpose.so" % XTERN_ROOT, os.R_OK):
        logging.error('thre is no "$XTERN_ROOT/dync_hook/interpose.so"')
        sys.exit(1)
    if not checkExist("%s/eval/rand-intercept/rand-intercept.so" % XTERN_ROOT, os.R_OK):
        logging.error('there is no "$XTERN_ROOT/eval/rand-intercept/rand-intercept.so"')
        sys.exit(1)
    XTERN_PRELOAD = "LD_PRELOAD=%s/dync_hook/interpose.so" % XTERN_ROOT
    RAND_PRELOAD = "L2D_PRELOAD=%s/eval/rand-intercept/rand-intercept.so" % XTERN_ROOT
    # set environment variable
    logging.debug("set timeformat to '\\nreal %E\\nuser %U\\nsys %S'")
    os.environ['TIMEFORMAT'] = "\nreal %E\nuser %U\nsys %S"

    # run command in shell, currently uses 'bash'
    bash_path = which('bash')
    if not bash_path:
        logging.critical("cannot find shell 'bash'")
        sys.exit(1)
    else:
        bash_path = bash_path[0]
        logging.debug("find 'bash' at %s" % bash_path)


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
        try:
            os.unlink('current')
        except OSError:
            pass
        os.symlink(run_dir, 'current')
        if not run_dir:
            continue
        os.chdir(run_dir)

        # write diff file if the repository is dirty
        if git_info[3]:
            with open("git_diff", "w") as diff:
                diff.write(git_info[3])
        
        benchmarks = local_config.sections()
        for benchmark in benchmarks:
            if benchmark == "default" or benchmark == "example":
                continue
            processBench(local_config, benchmark)

        os.chdir(root_dir)
       
