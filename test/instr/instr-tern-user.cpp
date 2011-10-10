// RUN: %llvmgcc %s -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -S -o %t2

// compare number of user-inserted calls and tern-translated calls
// RUN: grep tern_symbolic %t1.ll  | grep call  | grep -v llvm.dbg.declare | wc -l > %t2.count1
// RUN: grep tern_symbolic %t2-record.ll  | grep call | wc -l > %t2.count2
// RUN: diff %t2.count1 %t2.count2
// RUN: grep tern_task_begin %t1.ll  | grep call  | grep -v llvm.dbg.declare | wc -l > %t2.count1
// RUN: grep tern_task_begin %t2-record.ll  | grep call | wc -l > %t2.count2
// RUN: diff %t2.count1 %t2.count2
// RUN: grep tern_task_end %t1.ll  | grep call  | grep -v llvm.dbg.declare | wc -l > %t2.count1
// RUN: grep tern_task_end %t2-record.ll  | grep call | wc -l > %t2.count2
// RUN: diff %t2.count1 %t2.count2

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../../include/tern/user.h"

char buf[1024];
int x;
int main(int argc, char **argv) {

  int ret;

  read(1, buf, sizeof buf);
  tern_symbolic(&x, sizeof(x), "x");

  ret = recv(1, buf, sizeof(buf), 0);
  if(ret >= 0) {
    tern_task_begin(buf, ret, "req");
    tern_task_end();
  }

  return ret;
}
