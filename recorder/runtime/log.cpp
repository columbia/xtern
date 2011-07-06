#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logdefs.h"
#include "log.h"

// #define DEBUG

using namespace tern;

static __thread char* buf = NULL;
static __thread unsigned off = 0;

static __thread int fd = -1;
static __thread off_t foff = 0;

static __thread int tid = -1;

static inline void tern_log_map()
{
  if(buf)
    munmap(buf, TRUNK_SIZE);

  buf = (char*)mmap(0, TRUNK_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, off); 
  if(buf == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  off = 0;
  foff += TRUNK_SIZE;
}

static inline void check_log()
{
  // TODO: check log buf size and allocate new space if necessary
  assert(off + RECORD_SIZE  <= TRUNK_SIZE);
}

void tern_log_insid(int insid)
{
  check_log();
  *(int*)(buf+off) = insid | (InsidTy<<29);
  off += RECORD_SIZE;
}

void tern_log_loadstore(int insid, void* addr, uint64_t data)
{
  check_log();

  int len = 0;
  *(int*)(buf+off+len) = insid;
  len += sizeof insid;
  *(void**)(buf+off+len) = addr;
  len += sizeof addr;
  *(uint64_t*)(buf+off+len) = data;

  off += RECORD_SIZE;
}

void tern_log_call(int insid, short narg, void* func, ...)
{
  check_log();
}

void tern_log_ret(int insid, short narg, void* func, uint64_t data)
{
  check_log();

  int len = 0;
  short seq = NumExtraArgsRecord(narg) + 1;
  *(int*)(buf+off+len) = insid;
  len += sizeof insid;
  *(short*)(buf+off+len) = seq;
  len += sizeof seq;
  *(short*)(buf+off+len) = narg;
  len += sizeof narg;
  *(void**)(buf+off+len) = func;
  len += sizeof func;
  *(uint64_t*)(buf+off+len) = data;

  off += RECORD_SIZE;
}

void tern_log_init(void)
{
  char name[64];
  tid = 0; // TODO: get thread id from scheduler
  sprintf(name, "tern-log-tid-%d", tid); // TODO: get log dir
  fd = open(name, O_RDWR|O_CREAT, 0600);  assert(fd >= 0);
  ftruncate(fd, LOG_SIZE);
  tern_log_map();
}

void tern_log_exit(void)
{
  if(buf)
    munmap(buf, TRUNK_SIZE);
  if(fd >= 0)
    close(fd);
}

#ifdef DEBUG

int main()
{
  tern_log_init();

  for(int i=0; i<10000; ++i) {
    tern_log_i8 (0, (void*)0xdeadbeef, 0xAA);
    tern_log_i16(0, (void*)0xdeadbeef, 0xBBBB);
    tern_log_i32(0, (void*)0xdeadbeef, 0xCCCCCCCC);
    tern_log_i64(0, (void*)0xdeadbeef, 0xDDDDDDDDDDDDDDDD);
  }

  tern_log_exit();
  return 0;
}

#endif
