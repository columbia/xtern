#!/usr/bin/python

from utils import get_logfiles
import sys
import os

def printOp4SchedPrinter(op):
  """
    the op format of SchedPrinter: (id) (pid) (tid) (op) (round) [info]
  """
  opid = op['id']
  info = op.get('info', '')

  print '%s %s %s %s %s %s' % (str(opid), str(op['pid']), str(op['tid']), str(op['op']), str(op['turn']), info)

def printDep4SchedPrinter(x, y, info = None):
  """
    the dependency format of SchedPrinter: (id_from) (id_to) [info]
    generally we have id_from <= id_to
  """

  if (info == None):
    info = ""
  x = int(x)
  y = int(y)
  if (x > y):
    d = x
    x = y
    y = d
  print '%d %d %s' % (x, y, info)

def printDepGraph(ops, deps):
  print len(ops), len(deps)
  for op in ops:
    printOp4SchedPrinter(op)
  for dep in deps:
    (id_from, id_to, info) = dep
    printDep4SchedPrinter(id_from, id_to, info)

def readFromLogFile(filename, pid, tid):
  ret = []
  lines = open(filename, 'r').read().split('\n')
  keys = lines[0].split()   # the first line defines log format
  n = len(keys)   # number of record items
  assert(keys[n - 1] == 'args')
  for i in range(1, len(lines)):
    line = lines[i]
    tokens = line.split()
    if (len(tokens) < n - 1):   # the last item 'args' is optional
      print >> sys.stderr, 'warning: token number inconsistent, line %d of %s' % (i, filename)
      print >> sys.stderr, '    %s' % (line)
      continue

    op = dict()
    for k in range(0, n - 1):
      op[keys[k]] = tokens[k]
    op[keys[n - 1]] = ' '.join(tokens[(n-1):])

    if ('pid' not in op):
      op['pid'] = str(pid)
    assert(op['pid'] == str(pid))

    if ('tid' not in op):
      op['tid'] = str(tid)
    assert(op['tid'] == str(tid))

    ret.append(op)
  return ret

HANDLED_OPNAMES = {
  'pthread_create'                : 'P_C',
  'pthread_join'                  : 'P_J',
  'pthread_mutex_lock'            : 'M_L',
  'pthread_mutex_unlock'          : 'M_UL',
#  'pthread_barrier_wait'          : 'B_W',
  'pthread_barrier_init'          : 'B_I',
  'pthread_barrier_wait_first'    : 'B_W_F',
  'pthread_barrier_wait_second'   : 'B_W_S',
  'unknown'                       : 'unknown'
  }

def findMutexDeps(ops):
  """
  args format:
    M_L:     (mutex)
    M_UL:    (mutex)
  """
  MUTEX_OPS = ['M_L', 'M_UL']
  lastAccessed = dict()
  deps = []

  for op in ops:
    opname = op['op']
    if (opname not in MUTEX_OPS):
      continue
    args = op['args']
    opid = op['id']

    mutex = args
    mutex = op['pid'] + '_' + mutex   # separate process space
    if mutex in lastAccessed:
      deps.append((lastAccessed[mutex], opid, 'mutex=%s' % (mutex)))

    lastAccessed[mutex] = opid

  return deps

def findBarrierDeps(ops):
  """
  args format:
    B_I:      (barrier) (count)
    B_W_F:    (barrier)
    B_W_S:    (barrier)
  all numbers are printed in hex
  """
  TARGET_OPS = ['B_I', 'B_W_F', 'B_W_S']
  lastAccessed = dict()
  deps = []

  class BarrierType:
    def __init__(self, barrier, initop, count):
      self.initop = initop
      self.count = count
      self.arrived = []
      self.deps = dict()
      self.barrier = barrier

    def firstArrive(self, op):
      self.arrived.append((op['id'], op['tid']))

      if len(self.arrived) == self.count:
        # flush 
        opids = map(lambda x:int(x[0]), self.arrived)

        for x in self.arrived:
          tid = x[1]
          self.deps[tid] = opids

        self.arrived = []
      return []

    def secondArrive(self, op):
      opid = op['id']
      tid = op['tid']
      if tid not in self.deps:
        print op
      assert(tid in self.deps)

      ret = []
      for x in self.deps[tid]:
        ret.append((x, opid, 'barrier=%s' % self.barrier))
      self.deps[tid] = []
      return ret
  ### End of BarrierType ###

  barrierInfo = dict()

  for op in ops:
    opname = op['op']
    if (opname not in TARGET_OPS):
      continue

    args = op['args']
    opid = op['id']

    if (opname == 'B_I'):
      (barrier, count) = args.split()
      count = int(count, 16)  # hex number!
      barrier = op['pid'] + '_' + barrier
      barrierInfo[barrier] = BarrierType(barrier, opid, count)

    if (opname == 'B_W_F'):
      barrier = args
      barrier = op['pid'] + '_' + barrier
      assert(barrier in barrierInfo)
      info = barrierInfo[barrier]
      deps += info.firstArrive(op)

    if (opname == 'B_W_S'):
      barrier = args
      barrier = op['pid'] + '_' + barrier
      assert(barrier in barrierInfo)
      info = barrierInfo[barrier]
      deps += info.secondArrive(op)

  return deps

def findDeps(ops):
  """
    assuem opnames are converted to standard opnames by HANDLED_OPNAMES
    it returns tuples of (id_from, id_to, info)
  """

  ret = []

  ret += findMutexDeps(ops)
  ret += findBarrierDeps(ops)

  if False:
    for x in ret:
      (id_from, id_to, info) = x
      print ''
      print ops[id_from]
      print ops[id_to]
      print info

  return ret

def filterUnknownOps(ops):
  unhandled_opnames = set()
  for op in ops:
    opname = op['op']
    if (opname not in HANDLED_OPNAMES):
      # if so, we reset the opname to 'unknown', which is a protocol name for SchedPrinter
      op['op'] = 'unknown'
      unhandled_opnames.add(opname)
    else:
      # otherwise, we remap the opname to a shortened standard opname
      op['op'] = HANDLED_OPNAMES[opname]

  if (len(unhandled_opnames)):
    print >>sys.stderr, '%d unhandled opname(s)' % len(unhandled_opnames)
    print >>sys.stderr, '    ', unhandled_opnames

def parseLog(dirname, nopid = False):
  filenames = get_logfiles(dirname)
  print >>sys.stderr, 'filenames', filenames

  ops = []

  for filename in filenames:
    # filename format: 'tid-(pid)-(tid).txt'
    #                  or 'tid-(tid).txt' if nopid is set
    l = len(filename)
    prefix = filename[0 : (l - 4)]  # remove .txt
    tokens = prefix.split('-')
    if (nopid):
      pid = 0
      tid = int(tokens[1])
    else:
      pid = int(tokens[1])
      tid = int(tokens[2])

    proc_ops = readFromLogFile(os.path.join(dirname, filename), pid, tid)
    ops += proc_ops

  ops.sort(key = lambda x:int(x['turn']))
  # duplicated key check
  uniq_turn_count = len(set(map(lambda x:x['turn'], ops)))
  assert(uniq_turn_count == len(ops))

  # add an ID field
  for i in range(0, len(ops)):
    ops[i]['id'] = i
  
  filterUnknownOps(ops)
  deps = findDeps(ops)
  printDepGraph(ops, deps)

if __name__ == '__main__':
  args = sys.argv
  nopid = False
  for arg in args:
    if arg == "nopid":
      nopid = True

  parseLog('out', nopid = nopid)

