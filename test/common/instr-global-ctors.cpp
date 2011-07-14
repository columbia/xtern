// RUN: %llvmgcc %s -g -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -S > %t2.ll

struct S {
  S() {
    x = 10;
  }
  int x;
};

S s;

int main(int argc, char **argv) {
  return 0;
}
