// RUN: %llvmgcc %s -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -S > %t2.ll

#include <unistd.h>
#include <stdint.h>

int foo(char *p, int len) {
  for(int i = 0; i<len; ++i)
    p[i] = 10;
  return 0;
}

extern int bar(void);
extern int manyargs(char *p0, int len0, char *p1, int len1,
                    char *p2, int len2, char *p3, int len3,
                    char *p4, int len4, char *p5, int len5,
                    char *p6, int len6, char *p7, int len7);
extern void voidret(char* p, int len);
extern uint64_t int64ret(char* p, int len);

int main() {
  char buf[4096];
  int (*fp0)(char *, int);
  ssize_t (*fp1)(int, void*, size_t);

  // external
  read(1, buf, sizeof(buf));
  write(0, buf, sizeof(buf));

  // internal
  foo(buf, sizeof(buf));

  // indirect
  fp0 = &foo;
  fp0(buf, sizeof(buf));
  fp1 = &read;
  fp1(1, buf, sizeof(buf));

  void *fp2;

  fp2 = (void*)read;

  // no args
  bar();

  // many args
  manyargs(buf, sizeof(buf), buf, sizeof(buf),
           buf, sizeof(buf), buf, sizeof(buf),
           buf, sizeof(buf), buf, sizeof(buf),
           buf, sizeof(buf), buf, sizeof(buf));

  // void return
  voidret(buf, sizeof(buf));

  // int64 return
  int64ret(buf, sizeof(buf));

}
