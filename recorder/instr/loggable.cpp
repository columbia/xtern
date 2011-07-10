#include "util.h"
#include "common/instr/instrutil.h"
#include "loggable.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

namespace tern {

Value* getLoggable(const Instruction *I) {
  return getIntMetadata(I, "log");
}

void setLoggable(LLVMContext &C, Instruction *I) {
  Value *const data = ConstantInt::get(Type::getInt1Ty(C), 1);
  I->setMetadata("log", MDNode::get(C, &data, 1));
}

static bool loggableHelper(Instruction *ins) {
  Value *insid = getInsID(ins);
  if(!insid) return false;

  switch(ins->getOpcode()) {
  case Instruction::Load:
  case Instruction::Store:
    return true;
  case Instruction::Call:
  case Instruction::Invoke:
    return loggableCall(ins);
  }
  return false;
}

bool loggableInstruction(Instruction *ins) {
  Value *insid = getInsID(ins);
  if(!insid) return false;

  if(loggableHelper(ins))
    return true;

  BasicBlock *BB = ins->getParent();
  if(ins != BB->getFirstNonPHI())
    return false;

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
    return false;

  // if no other instructions in this BB is logged, must log FirstNonPHI
  BasicBlock::iterator ii = ins;
  for(++ii; ii!=BB->end(); ++ii)
    if(loggableHelper(ii))
      return false;
  return true;
}

//
//                              App+Lib            intrinsic   Tern    External
//                   has summaries   no summary
//  instr func?           No            Yes          No         No        No
//  instr call to func?   Yes           No           No         No        Yes

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
bool loggableFunc(Function *func) {
  if(func->getIntrinsicID() != Intrinsic::not_intrinsic)
    return false;

  if(func->getName().startswith("tern"))
    return false;

  if(func->isDeclaration())
    return false;

  return true;
}

bool loggableCallee(Function *func) {
  if(func->getIntrinsicID() != Intrinsic::not_intrinsic)
    return false;

  if(func->getName().startswith("tern"))
    return false;

  if(func->isDeclaration())
    return true;

  return false;
}

bool loggableCall(Instruction *call) {
  Value *insid = getInsID(call);
  if(!insid)
    return false;

  CallSite cs(call);

  Function *func = cs.getCalledFunction();
  if(func)
    return loggableCallee(func);

  // indirect call, must log
  // TODO: query alias and log only if may point to a loggable func
  return true;
}

} // namespace tern
