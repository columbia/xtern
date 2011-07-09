#include "util.h"
#include "loggable.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

namespace tern {

static Value* GetMetadata(const Instruction *I, const char* key) {
  MDNode *Node = I->getMetadata(key);
  if (!Node)
    return NULL;
  assert(Node->getNumOperands() == 1);
  ConstantInt *CI = dyn_cast<ConstantInt>(Node->getOperand(0));
  assert(CI);
  return CI;
}

Value* GetInsID(const Instruction *I) {
  return GetMetadata(I, "ins_id");
}

Value* GetLoggable(const Instruction *I) {
  return GetMetadata(I, "log");
}

void SetLoggable(LLVMContext &C, Instruction *I) {
  Value *const data = ConstantInt::get(Type::getInt1Ty(C), 1);
  I->setMetadata("log", MDNode::get(C, &data, 1));
}

static bool LoggableHelper(Instruction *ins) {
  Value *insid = GetInsID(ins);
  if(!insid) return false;

  switch(ins->getOpcode()) {
  case Instruction::Load:
  case Instruction::Store:
    return true;
  case Instruction::Call:
  case Instruction::Invoke:
    return LoggableCall(ins);
  }
  return false;
}

bool LoggableInstruction(Instruction *ins) {
  Value *insid = GetInsID(ins);
  if(!insid) return false;

  if(LoggableHelper(ins))
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
    if(LoggableHelper(ii))
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
bool LoggableFunc(Function *func) {
  if(func->getIntrinsicID() != Intrinsic::not_intrinsic)
    return false;

  if(func->getName().startswith("tern"))
    return false;

  if(func->isDeclaration())
    return false;

  return true;
}

bool LoggableCallee(Function *func) {
  if(func->getIntrinsicID() != Intrinsic::not_intrinsic)
    return false;

  if(func->getName().startswith("tern"))
    return false;

  if(func->isDeclaration())
    return true;

  return false;
}

bool LoggableCall(Instruction *call) {
  Value *insid = GetInsID(call);
  if(!insid)
    return false;

  CallSite cs(call);

  Function *func = cs.getCalledFunction();
  if(func)
    return LoggableCallee(func);

  // indirect call, must log
  // TODO: query alias and log only if may point to a loggable func
  return true;
}

} // namespace tern
