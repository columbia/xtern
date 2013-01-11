#include "test-run-queue.h"

/* A simple test file for the fast thread run queue. */

using namespace std;
run_queue q;

void print() {
  int i = 0;
  fprintf(stderr, "\n\nq size %u\n", (unsigned)q.size());
  for (run_queue::iterator itr = q.begin(); itr != q.end(); itr++) {
    fprintf(stderr, "q[%d] = %d, status = %d\n", i, *itr, itr->status);
    i++;
  }
}

int main(int argc, char *argv[]) {
  q.createThreadElem(0);
  q.createThreadElem(1);
  q.createThreadElem(2);
  q.createThreadElem(3);
  q.createThreadElem(4);

  q.push_back(0);
  q.push_back(1);
  q.push_back(2);
  q.push_back(3);
  q.push_back(4);
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

