#include <stdio.h>

int globalVar = 0;

void foo() {
  globalVar++;
}

/* Slicing target: the last return instruction, including the returned value.
    Testcase purpose: the not executed branch will modify shared variable; make 
    sure our algorithm will take this branch.
*/
int main (int argc, char *argv[]) {
  if (argc > 4) {
    foo();
  }
  return globalVar;
}

