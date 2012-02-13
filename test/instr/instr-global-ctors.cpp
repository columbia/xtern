// RUN: %llvmgcc %s -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -S -o %t2
// RUN: llc -o %t2.s %t2-record.ll
// RUN: %gxx -o %t2 %t2.s %ternruntime -lpthread -lrt
// RUN: ./%t2

// RUN: %llvmgcc %s -c -o %t1.ll -S
// add -with-uclibc to test global ctors/dtors handling when linking with uclibc
// RUN: %projbindir/tern-instr < %t1.ll -S -o %t2
// RUN: llc -o %t2.s %t2-record.ll
// RUN: %gxx -o %t2 %t2.s %ternruntime -lpthread -lrt
// RUN: ./%t2

#include <iostream>
using namespace std;

struct S {
  S() {
    cout << "S ctor\n";
    x = 10;
  }
  ~S() {
    x = 0;
    cout << "S dtor\n";
  }
  int x;
};

S s;

int main(int argc, char **argv) {
  return 0;
}
