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
from evalutils import mkDirP, copyFile, which, checkExist
from eval import *
from globals import G

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
    parrot_command = ' '.join(['time', G.XTERN_PRELOAD, export, exec_file] + inputs.split())

    default_options = getXternDefaultOptions()

    getLogWithEip(config, bench, default_options, parrot_command)
    
    os.chdir('..')
