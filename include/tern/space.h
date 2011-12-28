/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_SPACE_H
#define __TERN_SPACE_H

namespace tern {
namespace Space {

void enterSys(void); /// enter Sys space from App space
void exitSys(void); /// exit Sys space to App space
bool isApp(void); /// is current space App space?
bool isSys(void); /// is current space Sys space?

}
}

#endif
