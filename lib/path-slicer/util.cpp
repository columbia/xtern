#include "llvm/Target/TargetData.h"
#include "llvm/LLVMContext.h"
using namespace llvm;

#include "util.h"
using namespace tern;

Function *Util::getFunction(Instruction *instr) {
  return instr->getParent()->getParent();
}

BasicBlock *Util::getBasicBlock(Instruction *instr) {
  return instr->getParent();
}

const Function *Util::getFunction(const Instruction *instr) {
  return instr->getParent()->getParent();
}

const BasicBlock *Util::getBasicBlock(const Instruction *instr) {
  return instr->getParent();
}

/*
bool Util::isPHI(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isPHI(instr);
}

bool Util::isBr(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isBr(instr);
}

bool Util::isRet(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isRet(instr);
}

bool Util::isCall(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isCall(instr);
}

bool Util::isLoad(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isLoad(instr);
}

bool Util::isStore(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isStore(instr);
}

bool Util::isMem(DynInstr *dynInstr) {
  return isLoad(dynInstr) || isStore(dynInstr);
}
*/

bool Util::isPHI(const Instruction *instr) {
  return instr->getOpcode() == Instruction::PHI;
}

bool Util::isBr(const Instruction *instr) {
  return instr->getOpcode() == Instruction::Br;
}

bool Util::isUniCondBr(const Instruction *instr) {
  assert(isBr(instr));
  const BranchInst *bi = cast<BranchInst>(instr);
  return bi->isUnconditional();
}

bool Util::isRet(const Instruction *instr) {
  return instr->getOpcode() == Instruction::Ret;
}

bool Util::isCall(const Instruction *instr) {
  return instr->getOpcode() == Instruction::Call;
}

bool Util::isIntrinsicCall(const Instruction *instr) {
  const CallInst *ci = dyn_cast<CallInst>(instr);
  if (!ci)
    return false;
  const Function *callee = ci->getCalledFunction();
  return (callee && callee->isIntrinsic());
}

bool Util::isLoad(const Instruction *instr) {
  return instr->getOpcode() == Instruction::Load;
}

bool Util::isStore(const Instruction *instr) {
  return instr->getOpcode() == Instruction::Store;
}

bool Util::isMem(const Instruction *instr) {
  return isLoad(instr) || isStore(instr);
}

bool Util::hasDestOprd(const Instruction *instr) {
      /*  LLVM instructions that do not have dest operands.
      http://llvm.org/docs/LangRef.html#instref
        ret;
        br;
        switch;
        indirectbr;
        unwind;
        unreachable;
        store;
      */
      assert(isa<Instruction>(instr));
      if (instr->getOpcode() == Instruction::Store ||
        instr->getOpcode() == Instruction::Ret ||
        instr->getOpcode() == Instruction::Br ||
        instr->getOpcode() == Instruction::Switch ||
        instr->getOpcode() == Instruction::IndirectBr ||
        instr->getOpcode() == Instruction::Unwind ||
        instr->getOpcode() == Instruction::Unreachable
        )
        return false;
      if (instr->getOpcode() == Instruction::Call || instr->getOpcode() == Instruction::Invoke) {
        const Type *resultType = instr->getType();
        if (resultType == Type::getVoidTy(getGlobalContext()))
          return false;
      }
      return true;
    }

Value *Util::getDestOprd(llvm::Instruction *instr) {
  assert(hasDestOprd(instr));
  return dyn_cast<Value>(instr);
}

bool Util::isConstant(const llvm::Value *v) {
  if (isa<BasicBlock>(v)) // Filter out basic blocks, which are not "Constant" in LLVM.
    return true;
  else
    return isa<Constant>(v);
}

bool Util::isConstant(DynOprd *dynOprd) {
  const Value *v = dynOprd->getStaticValue();
  return isConstant(v);
}

bool Util::isThreadCreate(DynCallInstr *call) {
  Function *f = call->getCalledFunc();
  if (f && f->hasName() &&
    f->getNameStr().find("pthread_create") != std::string::npos)
    return true;
  else
    return false;
}

void Util::addTargetDataToPM(Module *module, PassManager *pm) {
  assert(module && pm);
  TargetData *TD = 0;
  const std::string &ModuleDataLayout = module->getDataLayout();
  if (!ModuleDataLayout.empty())
    TD = new TargetData(ModuleDataLayout);
  if (TD)
    pm->add(TD);
}

