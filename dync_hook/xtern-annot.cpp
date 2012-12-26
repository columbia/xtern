#include <stdio.h>

#include "tern/user.h"

#ifdef __cplusplus
extern "C" {
#endif

void tern_lineup_init(long opaque_type, unsigned count, unsigned timeout_turns) {
  //fprintf(stderr, "Non-deterministic tern_lineup_init\n");
}

void tern_lineup_start(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_start\n");
}

void tern_lineup_end(long opaque_type) {
  //fprintf(stderr, "Non-deterministic tern_lineup_end\n");
}

#ifdef __cplusplus
}
#endif
