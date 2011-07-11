/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGGABLE_H
#define __TERN_RECORDER_LOGGABLE_H

#include <vector>
#include "llvm/Instruction.h"

namespace tern {

llvm::Value* getLoggable(const llvm::Instruction *I);
void setLoggable(llvm::LLVMContext &C, llvm::Instruction *I);

bool loggableInstruction(llvm::Instruction *ins);

/// are instructions in @func loggable?
bool loggableFunc(llvm::Function *func);
/// is call to @func loggable?
bool loggableCallee(llvm::Function *func);
/// is this call loggable?
bool loggableCall(llvm::Instruction *call);

}

#endif
