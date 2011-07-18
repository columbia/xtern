/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGGABLE_H
#define __TERN_RECORDER_LOGGABLE_H

#include <vector>
#include "llvm/Instruction.h"

namespace tern {

/// NotLogged: instruction (or function body) is not logged at all
///
/// Logged: load or store instructions, calls to external non-sync or
///         summarized functions
//
/// LogSync: synchronization calls, logged, but the instrumentation is
///         handled by syncinstr, not loginstr, because replayer also
///         needs it
///
/// LogBBMarker: for marking instructions that purely for telling the
///         instruction log builder that a basic block is executed.  See
///         comments in instLogged() in loggable.cpp.  LogBBMarker can be
///         omitted if we use more sophisticated reconstruction algorithm.
///
enum LogTag {NotLogged, Logged, LogSync, LogBBMarker};

// LogTag getInstLoggedMD(const llvm::Instruction *I);
// void setInstLoggedMD(llvm::LLVMContext &C, llvm::Instruction *I, LogTag tag);

/// should we log the instruction?  can return all four LogTags
LogTag instLogged(llvm::Instruction *ins);

/// should we log instructions in @func?  return value can only be
/// NotLogged or Logged (i.e., can't be LogSync or LogBBMarker)
LogTag funcBodyLogged(llvm::Function *func);

/// Should we log calls to @func?  return value can only be NotLogged,
/// Logged, or LogSync (i.e., can't be LogBBMarker)
LogTag funcCallLogged(llvm::Function *func);

/// should we log this call ?  return value can only be NotLogged or
/// Logged (i.e., can't be LogBBMarker)
LogTag callLogged(llvm::Instruction *call);

}

#endif
