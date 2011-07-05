// RUN: %llvmgcc %s -O0 -c -o %t1.ll -S
// RUN: %opt < %t1.ll -load %llvmlibdir/id-manager.so -tag-id -S > %t2.ll
// RUN: %opt < %t2.ll -load %projlibdir/recinstr.so -loginstr -S > %t3.ll

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
