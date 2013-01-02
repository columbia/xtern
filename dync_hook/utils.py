#!/usr/bin/env python

import sys
import os
import shlex
import shutil
import glob

def subcall(cmd):
  print 'running cmd: ', cmd
  cmd = cmd + ' > cmdoutput.log'
  os.system(cmd)
  f = open('cmdoutput.log', 'r')
  ret = f.read()
  f.close()
  return ret

def get_logfiles(dir_name):
  files = os.listdir(dir_name)
  ret = []
  for file in files:
    if file.find('.txt') >= 0:
      ret.append(file)
  return ret

def get_block_ops():
  sync_file = os.path.join(os.environ['XTERN_ROOT'], 'include', 'tern', 'syncfuncs.def.h')
  cmd = 'grep BlockingSyscall {0} | grep -v / | cut -f 2- -d \'(\' | cut -f 1-1 -d ,'.format(sync_file)
  ret = subcall(cmd).split()
  print 'get_block_ops returns ', ret
  return ret

def proc_names(log_names):
  ret = []
  for name in log_names:
    tokens = name.split('-')
    pid = int(tokens[1])
    ret.append(pid)
  ret = sorted(list(set(ret)))
  return ret
