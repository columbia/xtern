#!/usr/bin/env python

import sys
import os
import shlex
import shutil
import glob

uniq_count = 0

def log_dir_name(num):
  return '_'.join(["xtern", "log", num])

def run_program(num, cmd):
  print "running test ", num
  os.system('./' + cmd)
#  os.system(' '.join("mv", "out", log_dir_name(num)));

def analyze_log(num):
  print "running analyzer on test ", num
  if not os.path.exists("out"):
    print 'cannot find log directory, by default it''s out'
    return False
#  m_hash_file = 'analyzing_' + str(num) + '.txt'
#  os.system('$XTERN_ROOT/install/bin/analyzer out -c > ' + m_hash_file)
#  os.system('cat ' + m_hash_file + ' >> analy.log')
  os.system(' '.join(['mv', 'out', 'out_' + str(num)]))

  my_log = 'out_' + str(num)
  found = False
  for i in range(0, num):
    out_file_name = 'ana_cmp_log_' + str(i) + '_' + str(num) + '.log'
    old_log = 'out_' + str(i)
    os.system('$XTERN_ROOT/install/bin/analyzer ' + my_log + ' -cmp ' + old_log + ' > ' + out_file_name + ' 2>&1 ')
    result = open(out_file_name, "r")
    text = result.read()
    result.close()
    if text.find('bipre-matches') >= 0:
      found = True
      break

  if not found:
    global uniq_count
    uniq_count = uniq_count + 1
    print 'uniq_count changed to ', uniq_count
  print 'uniq_count = ', uniq_count

  backup_dir = 'backup_log_' + str(num)
  os.system('mkdir ' + backup_dir)
  os.system('mv *.log ' + backup_dir + '/')

def gen_summary(nruns):
  print 'uniq_count = ', uniq_count
  print 'summary not implemented yet'

def initialize():
  os.system('rm -rf out*')
  os.system('rm -rf backup_log_*')
  os.system('rm -rf analy.log')
  os.system('rm -rf *.log')

def setup_tests(args):
  nruns = 50
  cmd = 'run.sh'
  initialize()
  if (len(args) > 0):
    nruns = int(args[0])
  if (len(args) > 1):
    cmd = args[1]
  
  for run_number in range(0, nruns):
    run_program(run_number, cmd)
    analyze_log(run_number)

  gen_summary(nruns);

if __name__ == '__main__':
  setup_tests(sys.argv[1:])
