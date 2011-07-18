/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "util.h"
#include "common/instr/instrutil.h"
#include "loggable.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

namespace tern {

// LogTag getInstLoggedMD(const Instruction *I) {
//   Value *MD = getIntMetadata(I, "log");
//   if(!MD)
//     return NotLogged;

//   ConstantInt *CI = dyn_cast<ConstantInt>(MD);
//   return (LogTag) CI->getZExtValue();
// }

// void setInstLoggedMD(LLVMContext &C, Instruction *I, LogTag tag) {
//   assert(tag == Logged || tag == LogBBMarker && "invalid Log MD Tag!");
//   Value *const data = ConstantInt::get(Type::getInt8Ty(C), tag);
//   I->setMetadata("log", MDNode::get(C, &data, /* number of values */ 1));
// }

static LogTag instLoggedHelper(Instruction *ins) {
  Value *insid = getInsID(ins);
  if(!insid) return NotLogged;

  switch(ins->getOpcode()) {
  case Instruction::Load:
  case Instruction::Store:
    return Logged;
  case Instruction::Call:
  case Instruction::Invoke:
    return callLogged(ins);
  }
  return NotLogged;
}

LogTag instLogged(Instruction *ins) {
  Value *insid = getInsID(ins);
  if(!insid) return NotLogged;

  LogTag tag = instLoggedHelper(ins);

  if(tag != NotLogged)
    return tag;

  BasicBlock *BB = ins->getParent();
  if(ins != BB->getFirstNonPHI())
    return NotLogged;

#if 0
  // can probably use fancier algorithm (e.g., dominators or
  // postdominators) to reduce the number of basic blocks logged.  i.e.,
  // give B1 dominates B2, if B2 is logged, B1 must have executed.
  // However, this complicates the reconstruction of instruction trace.
  // Let's keep it simple for now.
  //
  // single-entry, single-exit?
  int npred = 0, nsucc = 0;
  pred_iterator pi = pred_begin(BB);
  while(pi != pred_end(BB)) {
    ++ pi;
    ++ npred;
  }
  succ_iterator si = succ_begin(BB);
  while(si != succ_end(BB)) {
    ++ si;
    ++ nsucc;
  }
  if(npred == 1 && nsucc == 1)
    return NotLogged;
#endif

  // If no other instructions in this BB before the first non-logged call
  // instruction is logged, log FirstNonPHI so that it's easier to resolve
  // branches when we reconstruct the instruction trace based on the
  // load/store/external call log.
  //
  // Specifically, consider the example below:
  //
  //             BB0
  //            /   \  BB2 branch
  //           /     \   is taken
  //         BB1      BB2:
  //                   call foo (internal, so not logged)
  //
  // If we don't log any thing before the call to foo in BB2, the log may
  // look like BB0's instructions -> foo:entry -> foo:ret -> BB2.  It is
  // thus difficult to figure out where we went from BB0 without going
  // across function boundaries or recomputing the branch value.
  //
  // To simplify this task, we log a marker before the call to foo in BB2.
  //
  for(BasicBlock::iterator ii=ins; ii!=BB->end(); ++ii) {
    if(ii->getOpcode() == Instruction::Call) { // first call instruction
      LogTag tag = callLogged(ii);
      if(tag == NotLogged)  // if call not logged, log marker
        return LogBBMarker;
      else
        return NotLogged;
    }
    // if any inst before first call is logged, no marker
    if(instLoggedHelper(ii))
      return NotLogged;
  }
  return LogBBMarker;
}

//
//                              App+Lib            intrinsic   Tern    External
//                   has summaries   no summary
// func body logged?       No            Yes          No         No        No
// call to func logged?   Yes            No           No         No       Yes

// TODO: summarized function ==> true
//
// functions with body: app, lib, tern
// external functions without body
//
// if summary exists, use
// else
//   use conservative summary for ext
//   instr app and lib
//
// TODO: necessary to log intrinsic calls?
//
LogTag funcBodyLogged(Function *func) {
  if(func->getIntrinsicID() != Intrinsic::not_intrinsic)
    return NotLogged;

  if(func->getName().startswith("tern"))
    return NotLogged;

  if(func->isDeclaration())
    return NotLogged;

  return Logged;
}

LogTag funcCallLogged(Function *func) {
  if(func->getIntrinsicID() != Intrinsic::not_intrinsic)
    return NotLogged;

  if(func->getName().startswith("tern")) {
    assert(!func->getName().startswith("tern_log")
           && "Module already has recorder (LogInstr) instrumentation; "\
           "can only use this function on a module instrumented by LogInstr");
    return LogSync;
  }

  if(func->isDeclaration())
    return Logged;

  return NotLogged;
}


/// for every direct call, we check if the function is one of those whose
/// calls have to be logged (external or summarize).
///
/// for every indirect call, we instrument the call and check whether we
/// have to log it at runtime.  the invariant here is: for each indirect
/// call, either the first BB of the call target is logged, or there is a
/// call-extraargs-ret record for the call
///
LogTag callLogged(Instruction *call) {
  Value *insid = getInsID(call);
  if(!insid)
    return NotLogged;

  CallSite cs(call);

  Function *func = cs.getCalledFunction();
  if(func)
    return funcCallLogged(func);

  // indirect call
  //
  // TODO: can query alias and log only if may point to one of the
  // functions whose calls should be logged.  For now, instrument all
  // indirect calls and check if we need to log them at runtime
  return Logged;
}

} // namespace tern
