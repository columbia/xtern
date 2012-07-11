#include <assert.h>
#include <stdio.h>
#include "tern/space.h"

namespace tern{
namespace Space {

enum {Sys = false, App = true};

static __thread bool current_space = Sys; // always start in Sys space

/// cross from one space to another
static void cross(void) {
  current_space = !current_space;
}

bool setSpace(bool s)
{
  bool ret = current_space;
  current_space = s;
  return ret;
}

bool getSpace() {
  return current_space;
}

void enterSys(void) {
  assert(isApp() && "can't enter Sys space since already in Sys space!");
  cross();
}

void exitSys(void) {
  assert(isSys() && "can't exit Sys space since already in App space!");
  cross();
}

bool isApp(void) {
  return current_space == App;
}

bool isSys(void) {
  return current_space == Sys;
}


}
}
