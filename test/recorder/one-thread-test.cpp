// RUN: %llvmgcc %s -g -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -o %t2
// RUN: llvm-dis -f %t2-record.bc

// test the x86 .a libraries
// RUN: llc -o %t2.s %t2-record.bc
// RUN: g++ -g -o %t2 %t2.s -L %projlibdir -lcommonruntime -lrecruntime -lpthread
// RUN: ./%t2 | grep "this is a test. another test. "
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null

// stress
// RUN: ./%t2 && ./%t2 && ./%t2  && ./%t2  && ./%t2  && ./%t2  && ./%t2

// test the LLVM .bc modules
// RUN: llvm-ld -o %t3 %t2-record.bc %projlibdir/commonruntime.bc %projlibdir/recruntime.bc
// RUN: llc -o %t3.s %t3.bc
// RUN: g++ -g -o %t3 %t3.s -lpthread
// RUN: ./%t3 | grep "this is a test. another test. "
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null

// stress
// RUN: ./%t3 && ./%t3 && ./%t3  && ./%t3  && ./%t3  && ./%t3  && ./%t3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  char buf0[64], buf1[64], buf2[64];
  sprintf(buf1, "this is a test. ");
  sprintf(buf2, "another test. ");
  strcpy(buf0, buf1);
  strcat(buf0, buf2);
  printf("%s\n", buf0);
  return 0;
}
