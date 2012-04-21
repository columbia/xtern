#include <stdio.h>

int globalVar = 0;
int *p;
int *q;

void foo() {
  globalVar++;
}

/* Slicing target: the last return instruction, including the returned value.
    Testcase purpose: ret. Temporarily use the load/store testcase, since 
    they also contain ret instructions.
*/
int main (int argc, char *argv[]) {
  p = &globalVar;
  q = p;
  foo();
  if (argc == 0)
    (*q)++;
  foo();
  return globalVar;
}


