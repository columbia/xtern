#include <stdio.h>
#include <assert.h>
int x;

int main(int argc, char *argv[]) {
  x = argc;
  if (x == 7) {
    fprintf(stderr, "Has an assert failure.\n");
    assert(x != 8);
    return 1;
  } else {
    fprintf(stderr, "Good.\n");
    return 0;
  }
}

