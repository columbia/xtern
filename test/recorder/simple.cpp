// RUN: %llvmgcc %s -O0 -c -o -S > %t1.ll
// RUN: %opt < %t1.ll -load %projlibdir/recinstr.so -recinstr -S

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
