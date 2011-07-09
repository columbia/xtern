/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGGABLE_H
#define __TERN_RECORDER_LOGGABLE_H

#include <vector>
#include "llvm/Instruction.h"

namespace tern {

llvm::Value* GetInsID(const llvm::Instruction *I);
llvm::Value* GetLoggable(const llvm::Instruction *I);
void SetLoggable(llvm::LLVMContext &C, llvm::Instruction *I);

bool LoggableInstruction(llvm::Instruction *ins);

/// are instructions in @func loggable?
bool LoggableFunc(llvm::Function *func);
/// is call to @func loggable?
bool LoggableCallee(llvm::Function *func);
/// is this call loggable?
bool LoggableCall(llvm::Instruction *call);

}

#endif
