#include <stdio.h>

int globalVar = 0;
int globalVar2 = 0;


/* Slicing target: the last return instruction, including the returned value.
    Testcase purpose: phi....
*/
int main (int argc, char *argv[]) {
  // TBD: NOT SURE HOW TO GENERATE A LLVM PHI IN BC.
  return 0;
}

