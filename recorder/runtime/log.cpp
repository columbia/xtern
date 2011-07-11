/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <tr1/unordered_map>
#include "runtime-interface.h"
#include "logdefs.h"
#include "log.h"

using namespace std;
using namespace tern;

static __thread char* _buf = NULL;
static __thread unsigned _off = 0;

static __thread int _tid = -1;

static __thread int _fd = -1;
static __thread off_t _foff = 0;
static __thread char _logfile[64];

typedef tr1::unordered_map<void*, unsigned> func_map_t;
static func_map_t _loggable_callees;
static func_map_t _escape_callees;

static inline bool is_escape_callee(void* func) {
  return _escape_callees.find(func) != _escape_callees.end();
}

static inline bool is_loggable_callee(void* func) {
  return _loggable_callees.find(func) != _loggable_callees.end();
}

void tern_loggable_callee(void* func, unsigned funcid) {
  _loggable_callees[func] = funcid;
}

void tern_escape_callee(void* func, unsigned funcid) {
  _escape_callees[func] = funcid;
}

void tern_all_loggable_callees(void) __attribute((weak));
void tern_all_loggable_callees(void) {
  // empty function; will be replaced by loginstr
}

static inline void log_map() {
  if(_buf)
    munmap(_buf, TRUNK_SIZE);

  _buf = (char*)mmap(0, TRUNK_SIZE, PROT_WRITE|PROT_READ,
                     MAP_SHARED, _fd, _foff);

  if(_buf == MAP_FAILED) {
    perror("mmap");
    abort();
  }

  _off = 0;
  _foff += TRUNK_SIZE;
}

static inline void check_log() {
  // TODO: check log buffer size and allocate new space if necessary
  assert(_off + RECORD_SIZE  <= TRUNK_SIZE);
}

void tern_log_insid(int insid) {
  check_log();

  InsidRec *rec = (InsidRec*)(_buf+_off);
  rec->insid = insid;
  rec->type = InsidRecTy;

  _off += RECORD_SIZE;
}

static inline void log_loadstore(int type, int insid,
                                 void* addr, uint64_t data) {
}

void tern_log_load(int insid, void* addr, uint64_t data) {
  check_log();

  LoadRec *rec = (LoadRec*)(_buf+_off);
  rec->insid = insid;
  rec->type = LoadRecTy;
  rec->addr = addr;
  rec->data = data;

  _off += RECORD_SIZE;
}

void tern_log_store(int insid, void* addr, uint64_t data) {
  check_log();

  StoreRec *rec = (StoreRec*)(_buf+_off);
  rec->insid = insid;
  rec->type = StoreRecTy;
  rec->addr = addr;
  rec->data = data;

  _off += RECORD_SIZE;
}

void tern_log_call(int indir, int insid, short narg, void* func, ...) {

  if(indir && !is_escape_callee(func))
    return;

  check_log();

  short nextra = NumExtraArgsRecords(narg);
  short seq = 0;

  CallRec *call = (CallRec*)(_buf+_off);
  call->insid = insid;
  call->type = CallRecTy;
  call->seq = seq;
  call->narg = narg;
  assert(is_loggable_callee(func));
  call->funcid = _loggable_callees[func];

  short i, rec_narg;
  va_list vl;

  va_start(vl, func);

  // inlined args
  rec_narg = std::min(narg, (short)MAX_INLINE_ARGS);
  for(i=0; i<rec_narg; ++i)
    call->args[i] = va_arg(vl, uint64_t);

  _off += RECORD_SIZE;

  // extra args
  for(++seq; seq<=nextra; ++seq) {
    ExtraArgsRec *extra = (ExtraArgsRec*)(_buf+_off);
    extra->insid = insid;
    extra->type = ExtraArgsRecTy;
    extra->seq = seq;
    extra->narg = narg;

    short rec_i = 0;
    rec_narg = std::min(narg, (short)(MAX_INLINE_ARGS+MAX_EXTRA_ARGS*seq));
    while(i<rec_narg) {
      extra->args[rec_i] = va_arg(vl, uint64_t);
      ++ rec_i;
      ++ i;
    }
    _off += RECORD_SIZE;
  }
  va_end(vl);
}

void tern_log_ret(int indir, int insid, short narg, void* func, uint64_t data) {
  if(indir && !is_escape_callee(func))
    return;

  check_log();

  short seq = NumExtraArgsRecords(narg) + 1;

  ReturnRec *ret = (ReturnRec*)(_buf+_off);
  ret->insid = insid;
  ret->type = ReturnRecTy;
  ret->seq = seq;
  ret->narg = narg;
  assert(is_loggable_callee(func));
  ret->funcid = _loggable_callees[func];
  ret->data = data;

  _off += RECORD_SIZE;
}

void tern_log_begin() {
  tern_all_loggable_callees();
}

void tern_log_end() {
}

void tern_log_thread_begin(int tid) {
  _tid = tid;
  // TODO: get log output directory
  sprintf(_logfile, "tern-log-tid-%d", _tid);
  _fd = open(_logfile, O_RDWR|O_CREAT, 0600);  assert(_fd >= 0);
  ftruncate(_fd, LOG_SIZE);
  log_map();
}

const char* tern_log_name(void) {
  return _logfile;
}

void tern_log_thread_end(void) {
  if(_buf)
    munmap(_buf, TRUNK_SIZE);

  // truncate unused portion of log
  off_t size = _foff - TRUNK_SIZE + _off;
  ftruncate(_fd, size);

  if(_fd >= 0)
    close(_fd);

  _buf = NULL;
  _off = 0;
  _tid = 01;
  _fd = -1;
  _foff = 0;
}
