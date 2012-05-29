#include "llvm/Target/TargetData.h"
#include "llvm/LLVMContext.h"
#include "llvm/Metadata.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Support/CFG.h"
using namespace llvm;

#include "util.h"
using namespace tern;

static const std::string noLocation = "[No-Loc]";

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

bool Util::isProcessExitFunc(Function *f) {
  assert(f);
  return (f->getNameStr() == "exit" || f->getNameStr() == "_exit" ||
    f->getNameStr() == "abort" || f->getNameStr() == "__assert_fail");
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

bool Util::isGetElemPtr(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::GetElementPtr;
}

bool Util::isErrnoAddr(const Value *v) {
  const CallInst *ci = dyn_cast<CallInst>(v);
  if (!ci)
    return false;
  const Function *callee = ci->getCalledFunction();
  return (callee && callee->getNameStr() == "__errno_location");  // LLVM errno mechanism.
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

/*
We handle all the 12 conversion instructions from LLVM ref manual. They are all 
inheritated from CastInst.
7. Conversion Operations
'trunc .. to' Instruction
'zext .. to' Instruction
'sext .. to' Instruction
'fptrunc .. to' Instruction
'fpext .. to' Instruction
'fptoui .. to' Instruction
'fptosi .. to' Instruction
'uitofp .. to' Instruction
'sitofp .. to' Instruction
'ptrtoint .. to' Instruction
'inttoptr .. to' Instruction
'bitcast .. to' Instruction
*/
bool Util::isCastInstr(llvm::Value *v) {
  return isa<CastInst>(v);
}

Value *Util::stripCast(llvm::Value *v) {
  Value *retV = v;
  if (isCastInstr(v)) {
    retV = (cast<CastInst>(v))->getOperand(0);
    if (!isCastInstr(retV))
      return retV;
    else
      return stripCast(retV); // Recursive.
  }
  return retV;
}

std::string Util::printFileLoc(const Instruction *instr, const char *tag) {
  if (MDNode *N = instr->getMetadata("dbg")) {
    DILocation Loc(N);
    unsigned Line = Loc.getLineNumber();
    StringRef File = Loc.getFilename();
    //StringRef Dir = Loc.getDirectory();
    std::string str;
    raw_string_ostream OS(str);
    OS << " [" << tag << ":" << File << ":" << Line << "] ";
    return OS.str();
  }
  return noLocation;
}

std::string Util::printNearByFileLoc(const Instruction *instr) {
  static const int distance = 5; // 5 basicblocks distance.
  std::string tag = "L";
  if (instr->getMetadata("dbg"))
    return printFileLoc(instr, tag.c_str());
  else {
    // Lookup current basic block.
    BasicBlock *bb = (BasicBlock *)((long)instr->getParent());
    int curDistance = 0;
    tag = "B";
    do {
      // Find dbg in a bb.
      for (BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i) {
        if (i->getMetadata("dbg"))
          return printFileLoc(i, tag.c_str());
      }
	   tag = "A";
      // If can not find, find the successor, util hit end of a function.
      bool hasSuccessor = false;
  		for (succ_iterator it = succ_begin(bb); it != succ_end(bb); ++it) {
        bb = *it;
        hasSuccessor = true;
        break;
      }
      if (!hasSuccessor)
        return noLocation;
      curDistance++;
    } while (curDistance < distance);
  }
  return noLocation;
}

