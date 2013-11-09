// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime" -ternannotlib "%ternannotlib"  -ternbcruntime "%ternbcruntime"

/* A simple test file for the fast thread run queue. */

#include "../../include/tern/runtime/run-queue.h"

using namespace std;
using namespace tern;
run_queue q;

void print() {
  int i = 0;
  printf("q size %u\n", (unsigned)q.size());
  for (run_queue::iterator itr = q.begin(); itr != q.end(); itr++) {
    printf("q[%d] = %d, status = %d\n", i, *itr, itr->status);
    i++;
  }
}

int main(int argc, char *argv[]) {
  q.create_thd_elem(2);
  q.create_thd_elem(3);
  q.create_thd_elem(4);
  q.create_thd_elem(5);
  q.create_thd_elem(6);


  q.push_back(2);
  q.push_back(3);
  q.push_back(4);
  q.push_back(5);
  q.push_back(6);

  print();

 run_queue::iterator itr1 = q.begin();
 q.erase(itr1);
 print();


 run_queue::iterator itr2 = q.begin();
 ++itr2;
 q.erase(itr2);
 print();

 run_queue::iterator itr3 = q.end();
 q.erase(itr3);
 print();

 run_queue::iterator itr4 = q.begin();
 itr4++;
 itr4++;
 q.erase(itr4);
 print();


 int first = q.front();
 first = 99;
 print();

 q.push_front(4);
 print(); 

 q.pop_front();
 print(); 
}


// CHECK: q size 5
// CHECK-NEXT: q[0] = 2, status = 0
// CHECK-NEXT: q[1] = 3, status = 0
// CHECK-NEXT: q[2] = 4, status = 0
// CHECK-NEXT: q[3] = 5, status = 0
// CHECK-NEXT: q[4] = 6, status = 0
// CHECK-NEXT: q size 4
// CHECK-NEXT: q[0] = 3, status = 0
// CHECK-NEXT: q[1] = 4, status = 0
// CHECK-NEXT: q[2] = 5, status = 0
// CHECK-NEXT: q[3] = 6, status = 0
// CHECK-NEXT: q size 3
// CHECK-NEXT: q[0] = 3, status = 0
// CHECK-NEXT: q[1] = 5, status = 0
// CHECK-NEXT: q[2] = 6, status = 0
// CHECK-NEXT: q size 3
// CHECK-NEXT: q[0] = 3, status = 0
// CHECK-NEXT: q[1] = 5, status = 0
// CHECK-NEXT: q[2] = 6, status = 0
// CHECK-NEXT: q size 2
// CHECK-NEXT: q[0] = 3, status = 0
// CHECK-NEXT: q[1] = 5, status = 0
// CHECK-NEXT: q size 2
// CHECK-NEXT: q[0] = 3, status = 0
// CHECK-NEXT: q[1] = 5, status = 0
// CHECK-NEXT: q size 3
// CHECK-NEXT: q[0] = 4, status = 0
// CHECK-NEXT: q[1] = 3, status = 0
// CHECK-NEXT: q[2] = 5, status = 0
// CHECK-NEXT: q size 2
// CHECK-NEXT: q[0] = 3, status = 0
// CHECK-NEXT: q[1] = 5, status = 0

