#!/usr/bin/env python

import ConfigParser
import argparse
import os
import sys
import errno
import logging
import coloroutput
import re
import time
import numpy as np
import linecache
import difflib
import subprocess
import tarfile
from elftools.elf.elffile import ELFFile
from evalutils import mkDirP, copyFile, which, checkExist, copyDir
from eval import getGitInfo, genRunDir, getXternDefaultOptions
from globals import G

def getLogFiles(dirname):
    files = os.listdir(dirname)
    ret = []
    for file in files:
        file_name, file_extension = os.path.splitext(file)
        if file_extension == '.txt':
            ret.append(file)
    return ret

def generateLocalOptions(options, default_options, overwrite_options = {}):
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
        output += '%s\n' % entry
    with open("local.options", "w") as option_file:
        option_file.write(output)

# can define a class for op
def readLog(filename, pid, tid):
    lines = open(filename, 'r').readlines()
    keys = lines[0].split()
    n = len(keys)
    ret = []
    for i in range(1, len(lines)):
        line = lines[i]
        tokens = line.split()
        if (len(tokens) < n-1 ):
            logging.warning('token number inconsistent, line %d of %s' % (i, filename))
            continue

        op = dict()
        for j in range(0, n-1):
            op[keys[j]] = tokens[j]
        op[keys[n - 1]] = ' '.join(tokens[(n-1):])

        assert(op['tid'] == str(tid))

        op['pid'] = str(pid)
        
        ret.append(op)
    return ret


def filterOutIdle(ops):
    ret = []
    for op in ops:
        if op['tid'] == '1':
            continue
        if op['insid'].startswith('0xdead'):
            if op['insid'] == '0xdeadbeaf' or op['insid'] == '0xdead0000' or op['insid'] == '0xdeadffff':
                continue
        ret.append(op)
    return ret

def parseLogs(dirname, idleThread = False):
    files = getLogFiles(dirname)
    logging.debug(str(files))
    ops = []
    for filename in files:
        logname = os.path.splitext(filename)[0]
        tokens = logname.split('-')
        pid = int(tokens[1])
        tid = int(tokens[2])

        proc_ops = readLog(os.path.join(dirname, filename), pid, tid)
        ops += proc_ops

    if idleThread == False:
        ops = filterOutIdle(ops)

    ops.sort(key = lambda x:int(x['turn']))
    return ops

def removeKeys(ops, keys):
    for op in ops:
        for key in keys:
            del op[key]
    return ops

def removeOps(ops, unwanted_ops):
    ret = []
    for op in ops:
        if op['op'] in unwanted_ops:
            continue
        ret.append(op)
    return ret

def selectAttribute(ops, targetOp = '', sid = False, args = False, argsNum = []):
    for op in ops:
        if op['op'] != targetOp:
            continue
        if args:
            op['args'] = ' '.join(list(op['args'].split()[i] for i in argsNum))
        else:
            del op['args']
        if sid:
            pass
        else:
            del op['insid']
    return ops

class opMarker:
    def __init__(self):
        self.op = ""
        self.signature = ""
    def __init__(self, _op, _signature):
        self.op = _op
        self.signature = _signature
    def __repr__(self):
        #return 'syncop: %s signature: %s' % (self.op, self.signature)
        return '{:<25} {:>11}'.format(self.op, self.signature)
    def __cmp__(self, other):
        if not isinstance(other, opMarker):
            return NotImplemented
        return  (self.op == other.op and self.signature == other.signature)
    def __eq__(self, other):
        return self.op == other.op and self.signature == other.signature
    def __hash__(self):
        return hash(self.op) ^ hash(self.signature)

class opRecord:
    def __init__(self):
        self.tidList = []
        self.tidAccu = {}
        self.schedTime = []
    def __init__(self, tid, sched_time = 0):
        self.tidList = [tid]
        self.tidAccu= {tid: 1}
        self.schedTime = [int(sched_time)]
    def __repr__(self):
        return str(self.tidAccu) + str(self.getSchedTimeAvg())
    def getSchedTimeAvg(self):
        return np.average(self.schedTime)
    def getSchedTimeStd(self):
        return np.std(self.schedTime)
    def addTid(self, tid, sched_time = 0):
        if tid in self.tidAccu:
            self.tidAccu[tid] += 1
            self.schedTime.append(sched_time)
        else:
            self.tidList.append(tid)
            self.tidAccu[tid] = 1
            self.schedTime.append(sched_time)

def getOpSignature(op):
    signature = ''
    if 'insid' in op:
        if 'args' in op:
            signature = ' '.join([op['insid'], op['args']])
        else:
            signature = op['insid']
    elif 'args' in op:
        signature = op['args']
    else:
        logging.warning('%s cannot get signature' % op)
    return opMarker(op['op'], signature)
        

def removeBySignature(seen, op, signature):
    x = opMarker(op, signature)
    if x in seen:
        del seen[x]

def mergeOps(ops):
    ret = []
    for op in ops:
        if op['op'].endswith('_second'):
            continue
        if op['op'].endswith('_first'):
            op['op'] = op['op'][:-6]
        ret.append(op)
    return ret

def splitByNumOfRelatedThread(seen):
    possible = {}
    lonely = {}
    for op in seen:
        if len(seen[op].tidList) < 2:
            lonely[op] = seen[op]
        else:
            possible[op] = seen[op]
    return possible, lonely

def convertTime(ops, time_tag):
    for op in ops:
        time = op[time_tag].split(':')
        converted = int(time[0])*1000000000 + int(time[1])
        op[time_tag] = converted
    return ops


def findUniqueEip(idfun = None):
    ops = parseLogs('weip', idleThread = False)
    ops = removeKeys(ops, ['app_time', 'syscall_time', 'pid'])
    ops = removeOps(ops, ['tern_thread_end'])
    ops = convertTime(ops, 'sched_time')
    ops = mergeOps(ops) # merge _first, _second

    # tern_thread_begin: args[1] is the function pointer
    ops = selectAttribute(ops, 'tern_thread_begin', args = True, argsNum = [1])
    ops = selectAttribute(ops, 'pthread_join', sid = True)
    ops = selectAttribute(ops, 'pthread_create', sid = True)
    ops = selectAttribute(ops, 'pthread_barrier_wait', sid = True)
    ops = selectAttribute(ops, 'pthread_mutex_init', sid = True)
    ops = selectAttribute(ops, 'pthread_mutex_lock', sid = True)
    ops = selectAttribute(ops, 'pthread_mutex_unlock', sid = True)
    ops = selectAttribute(ops, 'pthread_cond_wait', sid = True)
    ops = selectAttribute(ops, 'pthread_cond_broadcast', sid = True)
    ops = selectAttribute(ops, 'pthread_cond_signal', sid = True)
    ops = selectAttribute(ops, 'pthread_barrier_init', sid = True)
    ops = selectAttribute(ops, 'pthread_barrier_destroy', sid = True)

    ##
    ops = removeKeys(ops, ['turn'])

    if idfun is None:
        def idfun(x): return x

    seen = {}
    for op in ops:
        marker = idfun(op)
        if marker in seen:
            seen[marker].addTid(op['tid'], op['sched_time'])
        else:
            seen[marker] = opRecord(op['tid'], op['sched_time'])

    removeBySignature(seen, op = 'tern_thread_begin', signature = '0x0')
    possible_ops, lonely_ops = splitByNumOfRelatedThread(seen)

    #for k in possible_ops:
    #    print k, possible_ops[k]
    num_of_lonely_ops = len(lonely_ops)
    num_of_possible_ops = len(possible_ops)
    logging.info('Ignore %d location%s... since each sync-op used by only one thread.' % (num_of_lonely_ops, "s"[num_of_lonely_ops==1:]))
    logging.info('Find %d possible location%s.' % (num_of_possible_ops, "s"[num_of_possible_ops==1:]) )

    #sorted_key = sorted(possible_ops, key = lambda x:possible_ops[x].getSchedTimeAvg() , reverse = True)
    #print sorted(possible_ops)
    #for k in sorted_key:
    #    print '%s %d' % (k, possible_ops[k].getSchedTimeAvg())
    return possible_ops

def selectOpMarker(keys, ops):
    ret = []
    for k in keys:
        if k.op in ops:
            ret.append(k)
    return ret

def decodeFuncname(dwarf_info, address):
    for CU in dwarf_info.iter_CUs():
        for DIE in CU.iter_DIEs():
            try:
                if DIE.tag == 'DW_TAG_subprogram':
                    lowpc = DIE.attributes['DW_AT_low_pc'].value
                    highpc = DIE.attributes['DW_AT_high_pc'].value
                    if lowpc <= address <= highpc:
                        #file_id =  DIE.attributes['DW_AT_decl_file'].value
                        #top_DIE = CU.get_top_DIE()
                        #print top_DIE.tag
                        #print top_DIE.attributes['DW_AT_comp_dir'].value
                        return DIE.attributes['DW_AT_name'].value
            except KeyError:
                continue
    return None

def decodeFileLine(dwarf_info, address):
    for CU in dwarf_info.iter_CUs():
        line_prog = dwarf_info.line_program_for_CU(CU)
        prev_state = None
        for entry in line_prog.get_entries():
            if entry.state is None or entry.state.end_sequence:
                continue
            if prev_state and prev_state.address <= address < entry.state.address:
                file_name = line_prog['file_entry'][prev_state.file - 1].name
                #print line_prog['file_entry'][prev_state.file - 1]
                #print prev_state
                line = prev_state.line
                return file_name, line
            prev_state = entry.state
    return None, None

def decodeFileLocation(dwarf_info, file_name, address):
    candidate = []
    for CU in dwarf_info.iter_CUs():
        for DIE in CU.iter_DIEs():
            try:
                if DIE.tag == 'DW_TAG_compile_unit':
                    name = DIE.attributes['DW_AT_name'].value
                    if name == file_name:
                        candidate.append(DIE)
            except KeyError:
                continue
    if len(candidate) == 0:
        return None
    if len(candidate) == 1:
        #for k in candidate[0].attributes:
        #    print k
        return candidate[0].attributes['DW_AT_comp_dir'].value
    logging.warning('find %s at multiple locations..' % file_name)
    return None

def generatePatch(exec_file, address):
    address = int(address, 16)
    with open(exec_file, 'rb') as f:
        elf_file = ELFFile(f)

        if not elf_file.has_dwarf_info():
            logging.error('%s has no DWARF info.')
            sys.exit(1)

        dwarf_info = elf_file.get_dwarf_info()
        func_name = decodeFuncname(dwarf_info, address)
        file_name, line_number = decodeFileLine(dwarf_info, address)
        file_loc = decodeFileLocation(dwarf_info, file_name, address)

    #print func_name, file_name, line_number, file_loc
    file_path = os.path.join(file_loc, file_name)

    orig_file = open(file_path, 'r').readlines()
    patched_file = list(orig_file)
    patched_file.insert(line_number, 'soba_wait(0);\n')
    patch = difflib.unified_diff(orig_file, patched_file, fromfile=file_name, tofile=file_name, n=3)

    return patch
    

def testLocations(loc_candidate, exec_file):
    sorted_key = sorted(loc_candidate, key = lambda x:loc_candidate[x].getSchedTimeAvg() , reverse = True)
    for k in sorted_key:
        print '%s %d %d' % (k, loc_candidate[k].getSchedTimeAvg(), loc_candidate[k].getSchedTimeStd())

    sorted_key = selectOpMarker(sorted_key, ['tern_thread_begin'])

    for item in sorted_key[:10]: # use top ten
        # for tern_thread_begin, the signature is the function start address
        if item.op == 'tern_thread_begin':
            generatePatch(exec_file, item.signature)

def readGuessConfigFile(config_file):
    try:
        newConfig = ConfigParser.ConfigParser( {"WORD_DIR": "", 
                                                "BUILD_CMD": "",
                                                "CLEAN_CMD": "",
                                                "SOURCE_ROOT": "",
                                                "SOURCE_FILES": "",
                                                "SOURCE_FOLDERS": "",
                                                "INPUTS": "",
                                                "REQUIRED_FILES": "",
                                                "DOWNLOAD_FILES": "",
                                                "TARBALL": "",
                                                "GZIP": "",
                                                "EXPORT": "",
                                                "INIT_ENV_CMD": "",
                                                "PRE_EXEC_EVENT": "",
                                                "EVAL_ID": ""} )
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

def buildSource(options, suffix = ''):
    build_root = os.path.join(os.getcwd(), 'build' + suffix)
    mkDirP(build_root)
    os.chdir(build_root)
    
    # copy source files
    source_root = options['source_root']
    source_folders = options['source_folders']
    source_files = options['source_files']
    for folder in source_folders.split():
        dir_path = os.path.join(source_root, folder)
        copyDir(dir_path, folder)
    for source_file in source_files.split():
        file_path = os.path.join(source_root, source_file)
        file_dir = os.path.dirname(source_file)
        if file_dir:
            mkDirP(file_dir)
        copyFile(file_path, source_file)
    
    # build
    work_dir = options['work_dir']
    clean_cmd = options['clean_cmd']
    build_cmd = options['build_cmd']
    exec_build_root = work_dir
    os.chdir(exec_build_root)

    logging.debug('cleaning up before building...')
    with open(os.path.join(build_root, 'clean.log'), 'w') as log_file:
        subprocess.call(clean_cmd.split(), stdin=None, stdout=log_file, stderr=log_file)
    logging.debug('building...')
    with open(os.path.join(build_root, 'build.log'), 'w') as log_file:
        build_return_code = subprocess.call(build_cmd.split(), stdin=None, stdout=log_file, stderr=log_file)
        if build_return_code != 0:
            logging.warning('build command has exit code %d' % build_return_code)

    os.chdir(build_root)
    os.chdir('..')

def setExec(options, suffix = ''):
    exec_file = options['exec_file']
    file_name = os.path.basename(os.getcwd())
    file_path = os.path.join('build'+suffix, exec_file)
    if not checkExist(file_path, os.X_OK):
        logging.error('cannot find exec_file %s' % exec_file)
        sys.exit(1)
    try:
        os.unlink(file_name)
    except OSError:
        pass
    os.symlink(file_path, file_name)
    return os.path.abspath(file_name)

def setExecEnv(options):
    source_root = options['source_root']

    # copy files
    required_files = options['required_files']
    for f in required_files.split():
        logging.debug("copy required file: %s" % f)
        if not os.path.isabs(f):
            f = os.path.join(source_root, f)
        copyFile(f, os.path.basename(f))
    
    # extract files
    tar_balls = options['tarball']
    for f in tar_balls.split():
        logging.debug("extract tarball: %s" % f)
        if not os.path.isabs(f):
            f = os.path.join(source_root, f)
        try:
            tarfile.is_tarfile(f)
            with tarfile.open(f, 'r') as t:
                t.extractall()
        except IOError as e:
            logging.error(str(e))
            return False
        except tarfile.TarError as e:
            logging.error(str(e))
            return False
    gzips = options['gzip']
    for f in gzips.split():
        logging.debug("extract gzip: %s" % f)
        if not os.path.isabs(f):
            f = os.path.join(source_root, f)
        try:
            tarfile.is_tarfile(f)
            with tarfile.open(f, 'r:gz') as t:
                t.extractall()
        except IOError as e:
            logging.error(str(e))
            return False
        except tarfile.TarError as e:
            logging.error(str(e))
            return False
    init_env_cmd = options['init_env_cmd']
    if init_env_cmd:
        logging.debug("run environment setting cmd: %s" % init_env_cmd)
        os.system(init_env_cmd)
    return True
        
def getCommand(options, exec_file, preload = True):
    pre_exec_cmd = options['pre_exec_event']
    export = options['export']
    inputs = options['inputs']
    if preload:
        parrot_command = ' '.join(['time', G.XTERN_PRELOAD, export, exec_file] + inputs.split())
    return parrot_command, pre_exec_cmd

def getLogWithEip(options, exec_file):
    parrot_cmd, pre_exec_cmd = getCommand(options, exec_file)
    logging.info("get eip and turn number information")
    if pre_exec_cmd:
        logging.debug("pre-exec-event: %s" % pre_exec_cmd)
    logging.debug("preload command...: %s" %parrot_cmd)

    overwrite_options = { 'log_sync':'1',
                        'output_dir':'./eip',
                        'dync_geteip' : '1',
                        'enforce_annotations': '0',
                        'whole_stack_eip_signature': '0'}
    generateLocalOptions(options, G.default_options, overwrite_options)
    with open('eip.log', 'w', 102400) as log_file:
        if pre_exec_cmd:
            os.system(pre_exec_cmd)
        proc = subprocess.Popen(parrot_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                shell=True, executable=G.BASH_PATH, bufsize = 102400, preexec_fn=os.setsid)
        proc.wait()
    
    overwrite_options['whole_stack_eip_signature'] = '1'
    overwrite_options['output_dir'] = './weip'
    generateLocalOptions(options, G.default_options, overwrite_options)
    with open('weip.log', 'w', 102400) as log_file:
        if pre_exec_cmd:
            os.system(init_env_cmd)
        proc = subprocess.Popen(parrot_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                shell=True, executable=G.BASH_PATH, bufsize = 102400, preexec_fn=os.setsid)
        proc.wait()

class patchReport:
    def __init__(self):
        self.nondet = -1
        self.parrot = -1
    def __init__(self, _nondet, _parrot):
        self.nondet = _nondet
        self.parrot = _parrot
    def __repr__(self):
        ret = 'nondet: %f\nparrot: %f' % (self.nondet, self.parrot)
        return ret

def createBaseline(options):
    eval_id = options['eval_id']
    if not eval_id:
        logging.error("TODO...")
        return NotImplemented
    nondet_perf = -1
    parrot_perf = -1
    for line in open(os.path.join(G.XTERN_ROOT, 'guess', 'base_nondet'), 'r').readlines():
        entries = line.split()
        if entries[0] == eval_id:
            nondet_perf = float(entries[2])
            break

    for line in open(os.path.join(G.XTERN_ROOT, 'guess', 'base_parrot'), 'r').readlines():
        entries = line.split()
        if entries[0] == eval_id:
            parrot_perf = float(entries[2])
            break

    return patchReport(nondet_perf, parrot_perf)

def findHints(options):
    buildSource(options, 'origin')
    exec_file= setExec(options, 'origin')

    if not setExecEnv(options):
        logging.error("cannot set exec environment.... halt")
        sys.exit(1)

    base = createBaseline(options)
    #print base

    getLogWithEip(options, exec_file) 
    loc_candidate = findUniqueEip(getOpSignature)
    testLocations(loc_candidate, exec_file)

if __name__ == "__main__":
    logger = logging.getLogger()
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s",
                                  "%Y%b%d-%H:%M:%S")
    ch = logging.StreamHandler()
    ch.setFormatter(formatter)
    ch.setLevel(logging.DEBUG)
    logger.addHandler(ch)
    logger.setLevel(logging.DEBUG)

    parser = argparse.ArgumentParser(
        description="Evaluate the perforamnce of Parrot")
    parser.add_argument('filename', nargs='*',
        type=str,
        default = ["build.cfg"],
        help = "list of configuration files (default: parrot.cfg)")
    args = parser.parse_args()

    if args.filename.__len__() == 0:
        logging.critical('no configuration file specified??')
        sys.exit(1)
    elif args.filename.__len__() == 1:
        logging.debug('config file: ' + ''.join(args.filename))
    else:
        logging.debug('config files: ' + ', '.join(args.filename))

    try:
        G.XTERN_ROOT = os.environ["XTERN_ROOT"]
        logging.debug('XTERN_ROOT = ' + G.XTERN_ROOT)
    except KeyError as e:
        logging.error("Please set the environment variable " + str(e))
        sys.exit(1)
    G.XTERN_APPS = os.path.abspath(G.XTERN_ROOT + "/apps/")
    logging.debug("set timeformat to '\\nreal %E\\nuser %U\\nsys %S'")
    os.environ['TIMEFORMAT'] = "\nreal %E\nuser %U\nsys %S"
    G.BASH_PATH = which('bash')
    if not G.BASH_PATH:
        logging.error("cannot find shell 'bash")
        sys.exit(1)
    else:
        G.BASH_PATH = G.BASH_PATH[0]
        logging.debug("find 'bash' at %s" % G.BASH_PATH)

    # check xtern files
    if not checkExist("%s/dync_hook/interpose.so" % G.XTERN_ROOT, os.R_OK):
        logging.error('thre is no "$G.XTERN_ROOT/dync_hook/interpose.so"')
        sys.exit(1)
    G.XTERN_PRELOAD = "LD_PRELOAD=%s/dync_hook/interpose.so" % G.XTERN_ROOT

    git_info = getGitInfo()
    G.default_options = getXternDefaultOptions()
    root_dir = os.getcwd()

    for config_file in args.filename:
        logging.info("processing '" + config_file + "'")
        config_full_path = os.path.abspath(config_file)
        if not checkExist(config_full_path, os.R_OK):
            logging.warning("skip " + config_full_path)
            continue
        local_config = readGuessConfigFile(config_full_path)
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
            config_options = dict((x , y) for x, y in local_config.items(benchmark))
            eval_id = config_options['eval_id']
            benchmark_root_dir = ''
            if eval_id:
                benchmakr_root_dir = '%s_' % eval_id
            segs = re.sub(r'(\")|(\.)|/|\'', '', benchmark).split()
            benchmark_root_dir += '_'.join(segs)
            mkDirP(benchmark_root_dir)
            os.chdir(benchmark_root_dir) 

            findHints(config_options)

            os.chdir('..')
        os.chdir(root_dir) 

#def find_hints_position(config, bench):
#    """
#    Set the execution environment.
#    """
#    logging.debug("processing: " + bench)
#    apps_name, exec_file = extractAppsExec(bench, G.XTERN_APPS)
#    if not checkExist(exec_file, os.X_OK):
#        logging.warning('%s does not exist, skip [%s]' % (exec_file, bench))
#        return
#    segs = re.sub(r'(\")|(\.)|/|\'', '', bench).split()
#    inputs = config.get(bench, 'inputs')
#    repeats = config.get(bench, 'repeats')
#    export = config.get(bench, 'EXPORT')
#    eval_id = config.get(bench, 'EVAL_ID')
#    if eval_id:
#        dir_name = "%s_" % eval_id
#    else:
#        dir_name = ""
#    dir_name +=  '_'.join(segs)
#    mkDirP(dir_name)
#    os.chdir(dir_name)
#
#    preSetting(config, bench, apps_name)
#    os.chdir('/mnt/sdd/newhome/yihlin/xtern/guess/guess2013Apr24_013936_01cc29_dirty/10_parsec_swaptions')
#    parrot_command = ' '.join(['time', G.XTERN_PRELOAD, export, exec_file] + inputs.split())
#
#    logging.info(parrot_command)
#    copyFile(os.path.realpath(exec_file), os.path.basename(exec_file))
#
#    default_options = getXternDefaultOptions()
#
#    #getLogWithEip(config, bench, default_options, parrot_command)
#    loc_candidate = findUniqueEip(getOpSignature)
#    testLocations(loc_candidate, exec_file)
#    
#    os.chdir('..')
#

#def getFileScope(file_path, line_number, line_range = 0):
#    ret = ""
#    for i in range(line_number - line_range, line_number + line_range + 1):
#        ret += linecache.getline(file_path, i)
#    sys.stdout.write(ret)
#    return ret

