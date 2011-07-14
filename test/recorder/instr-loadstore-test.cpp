// RUN: %llvmgcc %s -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -S > %t2.ll

// Test load instrumentation
// RUN: grep tern_log_load %t2.ll | grep -v declare | wc -l > %t2.count1
// RUN: grep " load " %t2.ll | wc -l > %t2.count2
// RUN: diff %t2.count1  %t2.count2

// Test store instrumentation
// RUN: grep tern_log_store %t2.ll | grep -v declare | wc -l > %t2.count1
// RUN: grep " store " %t2.ll | wc -l > %t2.count2
// RUN: diff %t2.count1  %t2.count2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sum = 0;

void foo(int i) {
  sum += i;
}

struct S {
  int data[10];
};

struct S bar() {
  S s;
  memset(s.data, 0, sizeof s.data);
  return s;
}

int main() {
  int i;
  for(i=0; i<100; ++i)
    foo(i);
  printf("sum = %d\n", sum);
}
