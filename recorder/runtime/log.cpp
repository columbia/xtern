#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

// #define DEBUG

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

void tern_log(int insid, void* addr, uint64_t data)
{
  // TODO: check log buf size and allocate new space if necessary
  assert(off + sizeof(insid) + sizeof(addr) 
         + sizeof(data) <= TRUNK_SIZE);

  *(int*)(buf+off) = insid;
  off += sizeof insid;
  *(void**)(buf+off) = addr;
  off += sizeof addr;
  *(uint64_t*)(buf+off) = data;
  off += sizeof data;

  off = (off+LOG_ALIGN-1)&~(LOG_ALIGN-1);
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
