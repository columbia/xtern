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
#include <pthread.h>
#include "tern/runtime/scheduler.h"
#include "tern/runtime/loghooks.h"
#include "tern/runtime/record-log.h"

#ifdef _DEBUG_LOGGER
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

using namespace std;

namespace tern {

__thread Logger* Logger::the = NULL;

Logger::func_map Logger::funcsCallLogged;
Logger::func_map Logger::funcsEscape;

void Logger::logInsid(unsigned insid) {
  checkAndGrowLogSize();

  InsidRec *rec = (InsidRec*)(buf+off);
  rec->setInsid(insid);
  rec->type = InsidRecTy;

  off += RECORD_SIZE;
}

void Logger::logLoad(unsigned insid, char* addr, uint64_t data) {
  checkAndGrowLogSize();

  LoadRec *rec = (LoadRec*)(buf+off);
  rec->setInsid(insid);
  rec->type = LoadRecTy;
  rec->addr = addr;
  rec->data = data;

  off += RECORD_SIZE;
}

void Logger::logStore(unsigned insid, char* addr, uint64_t data) {
  checkAndGrowLogSize();

  StoreRec *rec = (StoreRec*)(buf+off);
  rec->setInsid(insid);
  rec->type = StoreRecTy;
  rec->addr = addr;
  rec->data = data;

  off += RECORD_SIZE;
}

void Logger::logCall(uint8_t flags, unsigned insid,
                     short narg, void* func, va_list vl) {
  if(flags&CallIndirect) {
    if(!funcEscape(func))
      return;
    flags |= CalleeEscape;
    // FIXME: LogInstr should set  noreturn attribute for escape functions
    if(func == (void*)(intptr_t)exit
       || func == (void*)(intptr_t)pthread_exit) {
      flags |= CallNoReturn;
    }
  }

  assert(funcCallLogged(func));

  checkAndGrowLogSize();

  short nextra = NumExtraArgsRecords(narg);
  short seq = 0;

  CallRec *call = (CallRec*)(buf+off);
  call->setInsid(insid);
  call->type = CallRecTy;
  call->flags = flags;
  call->seq = seq;
  call->narg = narg;
  call->funcid = funcsCallLogged[func];

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
    extra->setInsid(insid);
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

void Logger::logRet(uint8_t flags, unsigned insid,
                    short narg, void* func, uint64_t data){
  if(flags&CallIndirect) {
    if(!funcEscape(func))
      return;
    flags |= CalleeEscape;
    // FIXME
    if(func == (void*)(intptr_t)exit
       || func == (void*)(intptr_t)pthread_exit) {
      flags |= CallNoReturn;
    }
  }

  assert(funcCallLogged(func));
  assert(!(flags&CallNoReturn)
         && "Logging return from a function that doesn't return!");

  checkAndGrowLogSize();

  short seq = NumExtraArgsRecords(narg) + 1;

  ReturnRec *ret = (ReturnRec*)(buf+off);
  ret->setInsid(insid);
  ret->type = ReturnRecTy;
  ret->flags = flags;
  ret->seq = seq;
  ret->narg = narg;
  ret->funcid = funcsCallLogged[func];
  ret->data = data;

  off += RECORD_SIZE;
}

// TODO: record ret->timedout
void Logger::logSync(unsigned insid, unsigned short sync,
                     unsigned turn, bool after, ...) {
  checkAndGrowLogSize();
  assert(sync >= syncfunc::first_sync && sync < syncfunc::num_syncs
    && "trying to log unknown synchronization operation!");

  SyncRec *rec = (SyncRec*)(buf+off);
  rec->setInsid(insid);
  rec->type = SyncRecTy;
  rec->sync = sync;
  rec->turn = turn;
  rec->after = after;

  assert(NumSyncArgs(sync) <= (int)MAX_INLINE_ARGS);

  va_list args;
  va_start(args, after);
  for(int i=0; i<NumSyncArgs(sync); ++i)
    rec->args[i] = va_arg(args, uint64_t);
  va_end(args);

  off += RECORD_SIZE;
}

Logger::Logger(const char* logFile) {
  foff = 0;
  fd = open(logFile, O_RDWR|O_CREAT, 0600);
  dprintf("logFile = %s\n", logFile);
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

void Logger::threadBegin(const char *filename) {
  assert(filename);
  the = new Logger(filename);
  assert(the && "can't allocate memory for logger!");
  dprintf("Logger: new logger = %p\n", (void*)the);
}

void Logger::threadEnd(void) {
  delete Logger::the;
}

void Logger::progBegin(void) {
  tern_funcs_call_logged();
}

void Logger::progEnd(void) {
  // nothing
}

} // namespace tern


void __attribute((weak)) tern_funcs_call_logged(void) {
  // empty function; will be replaced by loginstr
}

void tern_func_call_logged(void* func, unsigned funcid, const char* name) {
  tern::Logger::markFuncCallLogged(func, funcid, name);
}

void tern_func_escape(void* func, unsigned funcid, const char* name) {
  tern::Logger::markFuncEscape(func, funcid, name);
}

void tern_log_insid(unsigned insid) {
  tern::Logger::the->logInsid(insid);
}

void tern_log_load (unsigned insid, char* addr, uint64_t data) {
  tern::Logger::the->logLoad(insid, addr, data);
}

void tern_log_store(unsigned insid, char* addr, uint64_t data) {
  tern::Logger::the->logStore(insid, addr, data);
}

void tern_log_call(uint8_t flags, unsigned insid,
                   short narg, void* func, ...) {
  va_list args;
  va_start(args, func);
  tern::Logger::the->logCall(flags, insid, narg, func, args);
  va_end(args);
}

void tern_log_ret(uint8_t flags, unsigned insid,
                  short narg, void* func, uint64_t ret) {
  tern::Logger::the->logRet(flags, insid, narg, func, ret);
}
