// RUN: %llvmgcc %s -g -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll > %t2.bc
// RUN: llvm-dis -f %t2.bc
// RUN: llvm-ld -o %t3 %t2.bc %projlibdir/commonruntime.bc %projlibdir/recruntime.bc
// RUN: llc -o %t3.s %t3.bc
// RUN: g++ -g -o %t3 %t3.s -lpthread
// RUN: ./%t3 | grep "this is a test. another test. "

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
}
