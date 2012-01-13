#include <stdio.h>

int globalVar = 0;

void foo(int argc) {
  if (argc > 4)
    globalVar++;
}

/* Slicing target: the last return instruction, including the returned value.
    Testcase purpose 1: the not executed branch will modify shared variable; make 
    sure our algorithm will take this branch.
    Testcase purpose 2: make sure whether the return instruction and the call 
    instruction to foo() will be taken.
*/
int main (int argc, char *argv[]) {
  foo(argc);
  return globalVar;
}

