/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGGABLE_H
#define __TERN_RECORDER_LOGGABLE_H

#include <vector>
#include "llvm/Instruction.h"

namespace tern {

/// LogBBMarker is for marking instructions that purely for telling the
/// instruction log builder that a basic block is executed.  See comments
/// in loggableInstruction() in loggable.cpp.  It can be omitted if we use
/// more sophisticated reconstruction algorithm.
enum LogMDTag {NotLogged = 0, Logged = 1, LogBBMarker = 2};

LogMDTag getInstLoggedMD(const llvm::Instruction *I);
void setInstLoggedMD(llvm::LLVMContext &C, llvm::Instruction *I, LogMDTag tag);

LogMDTag instLogged(llvm::Instruction *ins);

/// are instructions in @func loggable?  return value can only be
/// NotLogged or Logged (i.e., can't be LogBBMarker)
LogMDTag funcBodyLogged(llvm::Function *func);

/// is call to @func loggable?  return value can only be NotLogged or
/// Logged (i.e., can't be LogBBMarker)
LogMDTag funcCallLogged(llvm::Function *func);

/// is this call loggable?  return value can only be NotLogged or Logged
/// (i.e., can't be LogBBMarker)
LogMDTag callLogged(llvm::Instruction *call);

}

#endif
