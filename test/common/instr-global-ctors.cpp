// RUN: %llvmgcc %s -c -o %t1.ll -S
// need -dont-warn-escaped-functions to nuke stderr warning ...
// RUN: %projbindir/tern-instr < %t1.ll -S -o %t2 -dont-warn-escaped-functions

#include <iostream>
using namespace std;

struct S {
  S() {
    cerr << "S ctor\n";
    x = 10;
  }
  ~S() {
    x = 0;
    cerr << "S dtor\n";
  }
  int x;
};

S s;

int main(int argc, char **argv) {
  return 0;
}
