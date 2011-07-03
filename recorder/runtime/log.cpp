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

#define TRUNK_SIZE (16*1024*1024U)   // 16MB
#define LOG_SIZE (1*TRUNK_SIZE)

//__thread

static char* buf = NULL;
static unsigned off = 0;

static int fd = -1;
static off_t foff = 0;

static inline void xtern_log_map()
{
  if(buf)
    munmap(buf, TRUNK_SIZE);

  buf = (char*)mmap(0, TRUNK_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, off); 
  if(buf == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  fprintf(stderr, "mmap returns 0x%x", buf);

  off = 0;
  foff += TRUNK_SIZE;
}

template<typename T>
static inline void xtern_log_i(int insid, void* addr, T data)
{
  // TODO: allocate new log space
  assert(off + sizeof(insid) + sizeof(addr) 
         + sizeof(data) <= TRUNK_SIZE);

  //*(int*)(buf+off) = insid;
  memcpy(buf+off, &insid, sizeof insid);
  off += sizeof insid;
  *(void**)(buf+off) = addr;
  off += sizeof addr;
  *(T*)(buf+off) = data;
  off += sizeof data;
}

void xtern_log_i8(int insid, void* addr, unsigned char data)
{
  xtern_log_i<char>(insid, addr, data);
}

void xtern_log_i16(int insid, void* addr, unsigned short data)
{
  xtern_log_i<short>(insid, addr, data);
}

void xtern_log_i32(int insid, void* addr, unsigned int data)
{
  xtern_log_i<int>(insid, addr, data);
}

void xtern_log_i64(int insid, void* addr, unsigned long long data)
{
  xtern_log_i<long long>(insid, addr, data);
}

void xtern_log_init(void)
{
  char name[64];
  sprintf(name, "xtern-log-tid-%d", 0); // TODO: get thread id
  fd = open(name, O_RDWR|O_CREAT, 0600);  assert(fd >= 0);
  ftruncate(fd, LOG_SIZE);
  xtern_log_map();
}

void xtern_log_exit(void)
{
  fprintf(stderr, "xtern_log_exit()");
  if(buf)
    munmap(buf, TRUNK_SIZE);
  if(fd >= 0)
    close(fd);
}

#ifdef DEBUG

int main() 
{
  xtern_log_init();

  for(int i=0; i<10000; ++i) {
    xtern_log_i8 (0, (void*)0xdeadbeef, 0xAA);
    xtern_log_i16(0, (void*)0xdeadbeef, 0xBBBB);
    xtern_log_i32(0, (void*)0xdeadbeef, 0xCCCCCCCC);
    xtern_log_i64(0, (void*)0xdeadbeef, 0xDDDDDDDDDDDDDDDD);
  }

  xtern_log_exit();
  return 0;
}

#endif
