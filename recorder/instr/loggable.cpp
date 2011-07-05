#include "util.h"
#include "loggable.h"

using namespace llvm;
using namespace std;

namespace tern
{
  Value* getInsID(const Instruction *I)
  {
    MDNode *Node = I->getMetadata("ins_id");
    if (!Node)
      return NULL;
    assert(Node->getNumOperands() == 1);
    ConstantInt *CI = dyn_cast<ConstantInt>(Node->getOperand(0));
    assert(CI);
    return CI;
  }

  static bool loggableHelper(Instruction *ins)
  {
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

  bool loggable(Instruction *ins)
  {
    if(loggableHelper(ins))
      return true;

    BasicBlock *BB = ins->getParent();
    if(ins != BB->getFirstNonPHI())
      return false;

    // if no other instructions in this BB is logged, must log this ins
    BasicBlock::iterator ii = ins;
    for(++ii; ii!=BB->end(); ++ii)
      if(loggableHelper(ii))
        return false;
    return true;
  }

  bool loggableFunc(Function *func)
  {
    // TODO: summarized function ==> true

    return func->isDeclaration(); // external
  }

  bool loggableCall(Instruction *call)
  {
    Value *insid = getInsID(call);
    if(!insid) return false;

    CallSite cs(call);

    Function *func = cs.getCalledFunction();
    if(func)
      return loggableFunc(func);

    // indirect call, must log
    return true;
  }
}
