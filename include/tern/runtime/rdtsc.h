#ifndef __TERN_RECORD_RDTSC_H
#define __TERN_RECORD_RDTSC_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include "tern/options.h"

// Only support i386 and x86_64.
#if defined(__i386__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

//__thread
// per process.
struct sync_op_entry {
  unsigned tid;
  const char *op;
  const char *op_suffix;
  unsigned op_print_depth;
  unsigned long long clock;
  void *eip;
};

extern void process_rdtsc_log(void);
extern void record_rdtsc_op(const char *op_name, const char *op_suffix, int print_depth, void *eip);

#endif

