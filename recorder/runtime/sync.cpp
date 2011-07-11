/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <tr1/unordered_map>
#include "runtime-interface.h"
#include "log.h"
#include "common/runtime/helper.h"

using namespace std;

tern::tid_manager tid_manager;

void tern_prog_begin() {
  tern_log_begin();
}

void tern_prog_end() {
  tern_log_begin();
}

void tern_thread_begin(void *addr, int nbytes, const char *name) {
  tern_log_thread_begin(0);
}

void tern_thread_end() {
  tern_log_thread_end();
}

int tern_pthread_create(pthread_t *thread,  pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  int ret;
  ret = __tern_pthread_create(thread, attr, thread_func, arg);

  // TODO: save errno

  tid_manager.on_pthread_create(*thread);

  return ret;
}
