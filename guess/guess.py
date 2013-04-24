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
from evalutils import mkDirP, copyFile, which, checkExist
from eval import *
from globals import G

def getLogFiles(dirname):
    files = os.listdir(dirname)
    ret = []
    for file in files:
        file_name, file_extension = os.path.splitext(file)
        if file_extension == '.txt':
            ret.append(file)
    return ret

def generateLocalOptions(config, bench, default_options, overwrite_options = {}):
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
        output += '%s\n' % entry
    with open("local.options", "w") as option_file:
        option_file.write(output)

def getLogWithEip(config, bench, default_options, command):
    init_env_cmd = config.get(bench, 'INIT_ENV_CMD')

    overwrite_options = { 'log_sync':'1', 'output_dir':'./eip',
                         'dync_geteip' : '1', 'enforce_annotations': '0',
                        'whole_stack_eip_signature': '0'}
    generateLocalOptions(config, bench, default_options, overwrite_options)
    with open('eip.log', 'w', 102400) as log_file:
        if init_env_cmd:
            os.system(init_env_cmd)
        proc = subprocess.Popen(command, stdout=log_file, stderr=subprocess.STDOUT,
                shell=True, executable=G.BASH_PATH, bufsize = 102400, preexec_fn=os.setsid)
        proc.wait()
    
    overwrite_options['whole_stack_eip_signature'] = '1'
    overwrite_options['output_dir'] = './weip'
    generateLocalOptions(config, bench, default_options, overwrite_options)
    with open('weip.log', 'w', 102400) as log_file:
        if init_env_cmd:
            os.system(init_env_cmd)
        proc = subprocess.Popen(command, stdout=log_file, stderr=subprocess.STDOUT,
                shell=True, executable=G.BASH_PATH, bufsize = 102400, preexec_fn=os.setsid)
        proc.wait()

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
    #for op in ops:
    #    print op

    if idfun is None:
        def idfun(x): return x

    seen = {}
    #result = []
    for op in ops:
        marker = idfun(op)
        if marker in seen:
            seen[marker].addTid(op['tid'], op['sched_time'])
        else:
            seen[marker] = opRecord(op['tid'], op['sched_time'])
            #result.append(marker)

    removeBySignature(seen, op = 'tern_thread_begin', signature = '0x0')
    possible_ops, lonely_ops = splitByNumOfRelatedThread(seen)

    #for k in possible_ops:
    #    print k, possible_ops[k]
    logging.info('Ignore %d locations... since each sync-op used by only one thread.' % len(lonely_ops))
    logging.info('Find %d possible locations.' % len(possible_ops))

    sorted_key = sorted(possible_ops, key = lambda x:possible_ops[x].getSchedTimeAvg() , reverse = True)
    #print sorted(possible_ops)
    for k in sorted_key:
        print '%s %d' % (k, possible_ops[k].getSchedTimeAvg())

    

def find_hints_position(config, bench):
    """
    Set the execution environment.
    """
    logging.debug("processing: " + bench)
    apps_name, exec_file = extractAppsExec(bench, G.XTERN_APPS)
    if not checkExist(exec_file, os.X_OK):
        logging.warning('%s does not exist, skip [%s]' % (exec_file, bench))
        return
    segs = re.sub(r'(\")|(\.)|/|\'', '', bench).split()
    inputs = config.get(bench, 'inputs')
    repeats = config.get(bench, 'repeats')
    export = config.get(bench, 'EXPORT')
    eval_id = config.get(bench, 'EVAL_ID')
    if eval_id:
        dir_name = "%s_" % eval_id
    else:
        dir_name = ""
    dir_name +=  '_'.join(segs)
    mkDirP(dir_name)
    os.chdir(dir_name)

    preSetting(config, bench, apps_name)
    #os.chdir('/mnt/sdd/newhome/yihlin/xtern/guess/guess2013Apr23_155756_01cc29_dirty/2_parsec_bodytrack')
    parrot_command = ' '.join(['time', G.XTERN_PRELOAD, export, exec_file] + inputs.split())

    logging.info(parrot_command)
    copyFile(os.path.realpath(exec_file), os.path.basename(exec_file))

    default_options = getXternDefaultOptions()

    getLogWithEip(config, bench, default_options, parrot_command)
    findUniqueEip(getOpSignature)
    
    os.chdir('..')
