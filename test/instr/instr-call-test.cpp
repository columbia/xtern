// RUN: %llvmgcc %s -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr -instrument-instructions < %t1.ll -S -o %t2

// Test load instrumentation
// RUN: grep tern_log_load %t2-record.ll | grep -v declare | wc -l > %t2.count1
// RUN: grep " load " %t2-record.ll | wc -l > %t2.count2
// RUN: diff %t2.count1  %t2.count2

// Test store instrumentation
// RUN: grep tern_log_store %t2-record.ll | grep -v declare | wc -l > %t2.count1
// RUN: grep " store " %t2-record.ll | wc -l > %t2.count2
// RUN: diff %t2.count1  %t2.count2

// Test call and ret matches
// RUN: grep tern_log_call %t2-record.ll | grep -v declare | wc -l > %t2.count1
// RUN: grep tern_log_ret  %t2-record.ll | grep -v declare | wc -l > %t2.count2
// RUN: diff %t2.count1  %t2.count2

// TODO: test that external functions are instrumented

#include <unistd.h>
#include <stdint.h>

int foo(char *p, int len) {
  for(int i = 0; i<len; ++i)
    p[i] = 10;
  return 0;
}

extern int bar(void);
extern int manyargs(char *p0, int len0, char *p1, int len1,
                    char *p2, int len2, char *p3, int len3,
                    char *p4, int len4, char *p5, int len5,
                    char *p6, int len6, char *p7, int len7);
extern void voidret(char* p, int len);
extern uint64_t int64ret(char* p, int len);

int main(int argc, char *argv[], char *env[]) {
  char buf[4096];
  int (*fp0)(char *, int);
  ssize_t (*fp1)(int, void*, size_t);

  // external
  read(1, buf, sizeof(buf));
  write(0, buf, sizeof(buf));

  // internal, not instrumented
  foo(buf, sizeof(buf));

  // indirect
  fp0 = &foo;
  fp0(buf, sizeof(buf));
  fp1 = &read;
  fp1(1, buf, sizeof(buf));

  void *fp2;

  fp2 = (void*)read;

  // no args
  bar();

  // many args
  manyargs(buf, sizeof(buf), buf, sizeof(buf),
           buf, sizeof(buf), buf, sizeof(buf),
           buf, sizeof(buf), buf, sizeof(buf),
           buf, sizeof(buf), buf, sizeof(buf));

  // void return
  voidret(buf, sizeof(buf));

  // int64 return
  int64ret(buf, sizeof(buf));
}
