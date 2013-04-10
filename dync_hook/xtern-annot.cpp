#include <stdio.h>

#include "tern/user.h"

#ifdef __cplusplus
extern "C" {
#endif

void soba_init(long opaque_type, unsigned count, unsigned timeout_turns) {
  //fprintf(stderr, "Non-deterministic tern_lineup_init\n");
}

void soba_destroy(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_destroy\n");
}

void tern_lineup_start(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_start\n");
}

void tern_lineup_end(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_end\n");
}

void soba_wait(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup\n");
}

void pcs_enter() {
  //fprintf(stderr, "Non-deterministic tern_non_det_start\n");
}

void pcs_exit() {
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

void pcs_barrier_exit(int bar_id, int cnt) {
  //fprintf(stderr, "Non-deterministic tern_non_det_barrier_end\n");
}

#ifdef __cplusplus
}
#endif
