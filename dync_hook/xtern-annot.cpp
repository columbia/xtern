#include <stdio.h>

#include "tern/user.h"

#ifdef __cplusplus
extern "C" {
#endif

void tern_lineup_init(long opaque_type, unsigned count, unsigned timeout_turns) {
  //fprintf(stderr, "Non-deterministic tern_lineup_init\n");
}

void tern_lineup_destroy(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_destroy\n");
}

void tern_lineup_start(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_start\n");
}

void tern_lineup_end(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_end\n");
}

void tern_lineup(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup\n");
}

void tern_non_det_start() {
  //fprintf(stderr, "Non-deterministic tern_non_det_start\n");
}

void tern_non_det_end() {
  //fprintf(stderr, "Non-deterministic tern_non_det_end\n");
}

void tern_set_base_timespec(struct timespec *ts) {
  //fprintf(stderr, "Non-deterministic tern_set_base_timespec\n");
}

void tern_set_base_timeval(struct timeval *tv) {
  //fprintf(stderr, "Non-deterministic tern_set_base_timeval\n");
}

void tern_detach() {
  //fprintf(stderr, "Non-deterministic tern_detach\n");
}

void tern_non_det_barrier_end(int bar_id, int cnt) {
  //fprintf(stderr, "Non-deterministic tern_non_det_barrier_end\n");
}

#ifdef __cplusplus
}
#endif
