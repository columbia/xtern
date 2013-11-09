#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <assert.h>
using namespace std;

int main(int argc, char **argv) {
  std::string path = getenv("SMT_MC_ROOT");
  assert(path != "" && "SMT_MC_ROOT must not be NULL.");
  path += "/xtern/dync_hook/interpose_mc.so";
  setenv("LD_PRELOAD", path.c_str(), 1);
  execv(argv[1], &argv[1]);
  return 0;
}
