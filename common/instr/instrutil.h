/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
/* -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_INSTRUTIL_H
#define __TERN_COMMON_INSTRUTIL_H

#include "llvm/Instruction.h"

namespace tern {

llvm::Value* getIntMetadata(const llvm::Instruction *I, const char* key);
llvm::Value* getInsID(const llvm::Instruction *I);

}

#endif
