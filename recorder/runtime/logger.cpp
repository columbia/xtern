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
#include "common/runtime/scheduler.h"
#include "loghooks.h"
#include "logger.h"

#ifdef _DEBUG_RECORDER
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

using namespace std;

namespace tern {

__thread Logger* Logger::the = NULL;

Logger::func_map Logger::loggableCallees;
Logger::func_map Logger::escapeCallees;

void Logger::logInsid(unsigned insid) {
  checkAndGrowLogSize();
  checkAndSetInsid(insid);

  InsidRec *rec = (InsidRec*)(buf+off);
  rec->insid = insid;
  rec->type = InsidRecTy;

  off += RECORD_SIZE;
}

void Logger::logLoad(unsigned insid, void* addr, uint64_t data) {
  checkAndGrowLogSize();
  checkAndSetInsid(insid);

  LoadRec *rec = (LoadRec*)(buf+off);
  rec->insid = insid;
  rec->type = LoadRecTy;
  rec->addr = addr;
  rec->data = data;

  off += RECORD_SIZE;
}

void Logger::logStore(unsigned insid, void* addr, uint64_t data) {
  checkAndGrowLogSize();
  checkAndSetInsid(insid);

  StoreRec *rec = (StoreRec*)(buf+off);
  rec->insid = insid;
  rec->type = StoreRecTy;
  rec->addr = addr;
  rec->data = data;

  off += RECORD_SIZE;
}

void Logger::logCall(int indir, unsigned insid, short narg, void* func, va_list vl) {
  if(indir && !isEscapeCallee(func))
    return;

  checkAndGrowLogSize();
  checkAndSetInsid(insid);

  short nextra = NumExtraArgsRecords(narg);
  short seq = 0;

  CallRec *call = (CallRec*)(buf+off);
  call->insid = insid;
  call->type = CallRecTy;
  call->seq = seq;
  call->narg = narg;
  assert(isLoggableCallee(func));
  call->funcid = loggableCallees[func];

  short i, rec_narg;

  // inlined args
  rec_narg = std::min(narg, (short)MAX_INLINE_ARGS);
  for(i=0; i<rec_narg; ++i)
    call->args[i] = va_arg(vl, uint64_t);

  off += RECORD_SIZE;

  // extra args
  for(++seq; seq<=nextra; ++seq) {
    checkAndGrowLogSize();

    ExtraArgsRec *extra = (ExtraArgsRec*)(buf+off);
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
    off += RECORD_SIZE;
  }
}

void Logger::logRet(int indir, unsigned insid, short narg, void* func, uint64_t data){
  if(indir && !isEscapeCallee(func))
    return;

  checkAndGrowLogSize();
  checkAndSetInsid(insid);

  short seq = NumExtraArgsRecords(narg) + 1;

  ReturnRec *ret = (ReturnRec*)(buf+off);
  ret->insid = insid;
  ret->type = ReturnRecTy;
  ret->seq = seq;
  ret->narg = narg;
  assert(isLoggableCallee(func));
  ret->funcid = loggableCallees[func];
  ret->data = data;

  off += RECORD_SIZE;
}

void Logger::logSync(unsigned insid, unsigned short sync,
                     unsigned turn, bool after, ...) {
  checkAndGrowLogSize();
  checkAndSetInsid(insid);

  SyncRec *ret = (SyncRec*)(buf+off);
  ret->insid = insid;
  ret->type = SyncRecTy;
  ret->turn = turn;
  ret->after = after;

  assert(NumSyncArgs(sync) <= (int)MAX_INLINE_ARGS);

  va_list args;
  va_start(args, after);
  for(int i=0; i<NumSyncArgs(sync); ++i)
    ret->args[i] = va_arg(args, uint64_t);
  va_end(args);

  off += RECORD_SIZE;
}

Logger::Logger(int tid) {
  SetLogName(logFile, sizeof(logFile), tid);

  foff = 0;
  fd = open(logFile, O_RDWR|O_CREAT, 0600);
  assert(fd >= 0 && "can't open log file!");
  ftruncate(fd, LOG_SIZE);

  buf = NULL;
  mapLogTrunk();
}

Logger::~Logger() {
  if(buf)
    munmap(buf, TRUNK_SIZE);

  dprintf("unmmapped %p, size %u\n", buf, TRUNK_SIZE);

  // truncate unused portion of log
  off_t size = foff - TRUNK_SIZE + off;
  ftruncate(fd, size);

  if(fd >= 0)
    close(fd);

  buf = NULL;
  off = 0;
  fd = -1;
  foff = 0;
}

void Logger::mapLogTrunk(void) {
  if(buf)
    munmap(buf, TRUNK_SIZE);

  buf = (char*)mmap(0, TRUNK_SIZE, PROT_WRITE|PROT_READ,
                     MAP_SHARED, fd, foff);
  assert(buf!=MAP_FAILED && "can't map log file using mmap()!");
  dprintf("Logger: mmapped %p, size %u\n", buf, TRUNK_SIZE);
  off = 0;
  foff += TRUNK_SIZE;
}

void Logger::threadBegin(int tid) {
  the = new Logger(tid);
  assert(the && "can't allocate memory for logger!");
  dprintf("Logger: new logger = %p\n", (void*)the);
}

void Logger::threadEnd(void) {
  delete Logger::the;
}

void Logger::progBegin(void) {
  tern_all_loggable_callees();
}

void Logger::progEnd(void) {
  // nothing
}

} // namespace tern


void __attribute((weak)) tern_all_loggable_callees(void) {
  // empty function; will be replaced by loginstr
}

void tern_loggable_callee(void* func, unsigned funcid) {
  tern::Logger::markLoggableCallee(func, funcid);
}

void tern_escape_callee(void* func, unsigned funcid) {
  tern::Logger::markEscapeCallee(func, funcid);
}

void tern_log_insid(unsigned insid) {
  tern::Logger::the->logInsid(insid);
}

void tern_log_load(unsigned insid, void* addr, uint64_t data) {
  tern::Logger::the->logLoad(insid, addr, data);
}

void tern_log_store(unsigned insid, void* addr, uint64_t data) {
  tern::Logger::the->logStore(insid, addr, data);
}

void tern_log_call(int indir, unsigned insid, short narg, void* func, ...) {
  va_list args;
  va_start(args, func);
  tern::Logger::the->logCall(indir, insid, narg, func, args);
  va_end(args);
}

void tern_log_ret(int indir, unsigned insid,
                  short narg, void* func, uint64_t ret) {
  tern::Logger::the->logRet(indir, insid, narg, func, ret);
}
