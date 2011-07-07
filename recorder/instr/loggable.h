/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGGABLE_H
#define __TERN_RECORDER_LOGGABLE_H

#include <vector>
#include "llvm/Instruction.h"

namespace tern {

llvm::Value* GetInsID(const llvm::Instruction *I);
llvm::Value* GetLoggable(const llvm::Instruction *I);
void SetLoggable(llvm::LLVMContext &C, llvm::Instruction *I);

bool Loggable(llvm::Instruction *ins);
bool LoggableCallToFunc(llvm::Function *func);
bool LoggableCall(llvm::Instruction *call);

}

#endif
