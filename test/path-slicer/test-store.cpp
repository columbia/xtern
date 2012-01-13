#include <stdio.h>

int globalVar = 0;
int *p;
int *q;

void foo() {
  globalVar++;
}

/* Slicing target: the last return instruction, including the returned value.
    Testcase purpose: store instructions with poniters. The if branch should be 
    taken.
*/
int main (int argc, char *argv[]) {
  p = &globalVar;
  q = p;
  foo();
  if (argc == 0)
    (*q) = 5;
  foo();
  return globalVar;
}

