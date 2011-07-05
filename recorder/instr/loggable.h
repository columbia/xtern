/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGGABLE_H
#define __TERN_RECORDER_LOGGABLE_H

#include <vector>
#include "llvm/Instruction.h"

namespace tern
{
  llvm::Value* getInsID(const llvm::Instruction *I);
  bool loggable(llvm::Instruction *ins);
  bool loggableFunc(llvm::Function *func);
  bool loggableCall(llvm::Instruction *call);
}

#endif
