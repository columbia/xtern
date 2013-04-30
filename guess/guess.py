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
            logging.debug('\t%s' % entry)
        elif option in config_options:
            entry = option + ' = ' + config.get(bench, option)
            logging.debug('\t%s' % entry)
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

def parseLogs(dirname, idleThread = False, quite = False):
    files = getLogFiles(dirname)
    if not quite:
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
        self.addr = -1
    def __init__(self, tid, address, sched_time = 0):
        self.tidList = [tid]
        self.tidAccu= {tid: 1}
        self.schedTime = [int(sched_time)]
        self.addr = address
    def __repr__(self):
        return '{:<s} {:<s} {:<s}'.format(str(self.tidAccu), str(self.getSchedTimeAvg()), self.addr)
    def getSchedTimeAvg(self):
        return reduce(lambda x, y: x + y, self.schedTime) / len(self.schedTime)
    def getSchedTimeStd(self):
        from math import sqrt
        n, mean, std = len(self.schedTime), self.getSchedTimeAvg(), 0
        for a in self.schedTime:
            std = std + (a - mean)**2
        return sqrt(std / float(n-1))
    def addTid(self, tid, sched_time = 0):
        if tid in self.tidAccu:
            self.tidAccu[tid] += 1
            self.schedTime.append(sched_time)
        else:
            self.tidList.append(tid)
            self.tidAccu[tid] = 1
            self.schedTime.append(sched_time)
    def checkAddr(self, address):
        if (self.addr != address):
            logging.error("the signature matches to different addresses: %s %s" % (self.addr, address))
            sys.exit(1)

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

def mergeListsOfDict(l1, l2, key):
    merged = {}
    for item in l1+l2:
        if item[key] in merged:
            merged[item[key]].update(item)
        else:
            merged[item[key]] = item
    return [val for (_, val) in merged.items()]

def findUniqueEip(idfun = None):
    eip_ops = parseLogs('eip', idleThread = False)
    eip_ops = removeKeys(eip_ops, ['app_time', 'syscall_time', 'sched_time', 'pid', 'op', 'args', 'tid'])
    for op in eip_ops:
        op['addr'] = op.pop('insid')
    ops = parseLogs('weip', idleThread = False, quite = True)
    ops = removeKeys(ops, ['app_time', 'syscall_time', 'pid'])
    assert(len(eip_ops) == len(ops))
    ops = mergeListsOfDict(ops, eip_ops, 'turn')
    ops = removeOps(ops, ['tern_thread_end'])
    ops = convertTime(ops, 'sched_time')
    # TODO: how to evaluate the turn waiting time for (first, second)...
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

    ops = removeKeys(ops, ['turn'])

    # handle tern_thread_begin address
    for op in ops:
        if op['op'] == 'tern_thread_begin':
            op['addr'] = op['args']

    if idfun is None:
        def idfun(x): return x

    seen = {}
    for op in ops:
        marker = idfun(op)
        if marker in seen:
            seen[marker].addTid(op['tid'], op['sched_time'])
            seen[marker].checkAddr(op['addr'])
        else:
            seen[marker] = opRecord(op['tid'], op['addr'], op['sched_time'])

    #removeBySignature(seen, op = 'tern_thread_begin', signature = '0x0')
    possible_ops, lonely_ops = splitByNumOfRelatedThread(seen)

    num_of_lonely_ops = len(lonely_ops)
    num_of_possible_ops = len(possible_ops)
    logging.info('ignore %d location%s... since each sync-op used by only one thread.' % (num_of_lonely_ops, "s"[num_of_lonely_ops==1:]))
    logging.info('find %d possible location%s.' % (num_of_possible_ops, "s"[num_of_possible_ops==1:]) )

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

def headerRequired(orig_file):
    ''' Search for header file location
        return true if find the pattern
        (TODO) might fail if previous header is added behind the hints
    '''
    for line in orig_file:
        if re.search('tern/user.h', line):
            return False
    return True

def generatePatch(options, exec_file, address):
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
    if headerRequired(orig_file):
        logging.debug('insert... #include<tern/user.h>\n')
        patched_file.insert(0, '#include<tern/user.h>\n')

    # find file path
    target_file = []
    for root, dirs, files in os.walk('source'):
        if file_name in files:
            target_file.append(os.path.join(root, file_name))
    if len(target_file) != 1:
        if len(taget_file) == 0:
            logging.error('cannot find mathed source file %s' % file_name)
        else:
            logging.error('found multiple source files... %s' % str(target_file))
        sys.exit(1)
    target_file = target_file.pop()

    patch = difflib.unified_diff(orig_file, patched_file, fromfile=file_name, tofile=target_file, n=3)
    
    #print ''.join(patch)

    #for line in patch:
    #    sys.stdout.write(line)

    return patch
    

def buildWithPatches(loc_candidate, options, report, exec_file):
    sorted_key = sorted(loc_candidate, key = lambda x:loc_candidate[x].getSchedTimeAvg() , reverse = True)
    for k in sorted_key:
        logging.info('%s %d %d' % (k, loc_candidate[k].getSchedTimeAvg(), loc_candidate[k].getSchedTimeStd()))

    # duplicated ops
    #sorted_key = selectOpMarker(sorted_key, ['tern_thread_begin'])

    # top ten
    patch_counter = 1
    for item in sorted_key[:10]:
        if item.op == 'tern_thread_begin':
            patch = generatePatch(options, exec_file, loc_candidate[item].addr)
            report.patches.append(patchRecord(''.join(patch), item, item.op, loc_candidate[item].addr, patch_counter))
        else:
            continue
        patch_counter += 1

    # apply patches and compile
    for patch in report.patches:
        patch_filename = os.path.join(os.getcwd(), '%d.patch' % patch.id)
        with open(patch_filename, 'w') as f:
            f.write(patch.patch)            
        buildSource(options, os.path.join(os.getcwd(), 'source'), prefix = str(patch.id), patch = patch_filename)
        

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
                                                "REPEATS": "",
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

def copySource(options):
    copy_dst = os.path.join(os.getcwd(), 'source')
    mkDirP(copy_dst)
    os.chdir(copy_dst)

    # copy source files
    source_root = options['source_root']
    source_folders = options['source_folders']
    source_files = options['source_files']
    for folder in source_folders.split():
        if os.path.exists(folder):
            continue
        dir_path = os.path.join(source_root, folder)
        copyDir(dir_path, folder)
    for source_file in source_files.split():
        file_path = os.path.join(source_root, source_file)
        file_dir = os.path.dirname(source_file)
        if file_dir:
            mkDirP(file_dir)
        copyFile(file_path, source_file)
    os.chdir('..')
    return copy_dst


def buildSource(options, source_dir, suffix = '', prefix = '', patch = None):
    build_root = os.path.join(os.getcwd(), prefix + 'build' + suffix)

    if not os.path.exists(build_root):
        copyDir(source_dir, build_root)
        os.chdir(build_root)

        # apply patch
        if patch:
            with open(patch, 'r') as p:
                subprocess.call(['patch', '-p1'], stdin=p, stdout=sys.stdout, stderr=sys.stderr)
    else:
        os.chdir(build_root)
    
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

def setExec(options, suffix = '', prefix = ''):
    exec_file = options['exec_file']
    file_name = os.path.basename(os.getcwd())
    file_path = os.path.join(prefix+'build'+suffix, exec_file)
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
        command = ' '.join(['time', G.XTERN_PRELOAD, export, exec_file] + inputs.split())
    else:
        command = ' '.join(['time', export, exec_file] + inputs.split())
    return command, pre_exec_cmd

def getLogWithEip(options, exec_file):
    parrot_cmd, pre_exec_cmd = getCommand(options, exec_file)
    logging.info("get eip and turn number information")
    if pre_exec_cmd:
        logging.debug("pre-exec-event: %s" % pre_exec_cmd)
    logging.debug("preload command...: %s" % parrot_cmd)

    logging.debug('getting eip of all sync-ops...')
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
    
    logging.debug('getting whole-eip-signature of all sync-ops...')
    overwrite_options['whole_stack_eip_signature'] = '1'
    overwrite_options['output_dir'] = './weip'
    generateLocalOptions(options, G.default_options, overwrite_options)
    with open('weip.log', 'w', 102400) as log_file:
        if pre_exec_cmd:
            os.system(init_env_cmd)
        proc = subprocess.Popen(parrot_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                shell=True, executable=G.BASH_PATH, bufsize = 102400, preexec_fn=os.setsid)
        proc.wait()

class patchRecord:
    def __init_(self):
        self.patch = ''
        self.key = -1
        self.op = ''
        self.addr = ''
        self.id = -1
        self.run_time = []
    def __init__(self, _patch, _key, _op, _addr, _id):
        self.patch = _patch
        self.key = _key
        self.op = _op
        self.addr = _addr
        self.id = _id
        self.run_time = []
    def __repr__(self):
        return 'signature: {:<s}\nop: {:<s}\naddr: {:<s}\nperf.: {:<s}\npatch:\n{:<s}\n'.format(self.key.signature, self.op, self.addr, self.run_time, self.patch)

class patchReport:
    def __init__(self):
        self.nondet = -1
        self.parrot = -1
        self.patches = [] 
    def __init__(self, _nondet, _parrot):
        self.nondet = _nondet
        self.parrot = _parrot
        self.patches = []
    def __repr__(self):
        ret = 'nondet: %f\nparrot: %f\n' % (self.nondet, self.parrot)
        ret += 'patches : %d\n' % len(self.patches)
        for patch in self.patches:
            ret += '\n%s' %  str(patch)
        return ret

def createBaseline(options):
    '''Get baseline perfromance:
        1. from records
        2. (TODO) if cannot get the records, execute and store the result
    '''
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

def getEvalPatchPerf(options, dir_name = '.'):
    cost = []
    repeats = options['repeats']
    for i in range(1, int(repeats)+1):
        log_file_name = os.path.join(dir_name, '%d.log' % i)
        for line in reversed(open(log_file_name, 'r').readlines()):
            if re.search('^real [0-9]+\.[0-9][0-9][0-9]$', line):
                cost += [float(line.split()[1])]
                break
    return cost

def evalPatches(options, patch_report):
    repeats = options['repeats']
    for patch in patch_report.patches:
        exec_file = setExec(options, prefix = str(patch.id))
        parrot_cmd, pre_exec_cmd = getCommand(options, exec_file)

        overwrite_options = {}
        generateLocalOptions(options, G.default_options, overwrite_options)
        os.unlink('local.options')

        log_dir = str(patch.id) + 'run'
        mkDirP(log_dir)
        for i in range(int(repeats)+1):
            with open('%s/%d.log' % (log_dir, i), 'w') as log_file:
                if pre_exec_cmd:
                    os.system(pre_exec_cmd)
                proc = subprocess.Popen(parrot_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                        shell=True, executable='/bin/bash', preexec_fn=os.setsid, bufsize = 102400)
                proc.wait()
        patch.run_time += getEvalPatchPerf(options, log_dir)

    return None

def genReport(report):
    ret  = 'non-det runtime: {:f}\n'.format(report.nondet)
    ret += 'no-hint runtime: {:f}\n'.format(report.parrot)
    ret += 'no-hint overhead: {:.4%}\n'.format(report.parrot / report.nondet - 1.0)
    for patch in report.patches:
        mean = reduce(lambda x, y: x + y, patch.run_time) / len(patch.run_time)
        ret += '\t-----------------------------------\n'
        ret += '\tPATCH ID: {:>25d}\n'.format(patch.id)
        ret += '\tSYNC-OP:  {:>25s}\n'.format(patch.op)
        ret += '\toverhead: {:25.4%}\t\t{:s}\n'.format(mean/report.nondet - 1.0, patch.run_time)
        ret += '\t-----------------------------------\n'
    return ret


def findHints(options):
    source_dir = copySource(options)
    buildSource(options, source_dir, suffix = 'origin')
    #os.chdir('/mnt/sdd/newhome/yihlin/xtern/guess/build2013Apr30_042008_8a416b_dirty/swaptions')
    exec_file= setExec(options, suffix = 'origin')

    if not setExecEnv(options):
        logging.error("cannot set exec environment.... (halt)")
        sys.exit(1)

    patch_report = createBaseline(options)

    getLogWithEip(options, exec_file) 
    loc_candidate = findUniqueEip(getOpSignature)
    buildWithPatches(loc_candidate, options, patch_report, exec_file)
    evalPatches(options, patch_report)

    print genReport(patch_report)

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

