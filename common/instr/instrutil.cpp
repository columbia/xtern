/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "instrutil.h"
#include "llvm/Metadata.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace tern {

static IDManager *IDM = NULL;

void setIDManager(IDManager *idm) {
  IDM = idm;
}

IDManager *getIDManager(void) {
  assert(IDM && "IDManager hasn't been set!");
  return IDM;
}

Value* getIntMetadata(const Instruction *I, const char* key) {
  MDNode *Node = I->getMetadata(key);
  if (!Node)
    return NULL;
  assert(Node->getNumOperands() == 1);
  ConstantInt *CI = dyn_cast<ConstantInt>(Node->getOperand(0));
  assert(CI);
  return CI;
}

Value* getInsID(const Instruction *I) {
  return getIntMetadata(I, "ins_id");
}

bool funcEscapes(Function* F) {
  CallSite cs;
  for(Value::use_iterator ui=F->use_begin(), E=F->use_end();
      ui != E; ++ui) {
    Instruction *I = dyn_cast<Instruction>(*ui);
    if(!I) // used in any ways not as the function in a call ==> escape
      return true;
    switch(I->getOpcode()) {
    case Instruction::Call:
    case Instruction::Invoke: { // used in a call ...
      CallSite cs(I);
      for(CallSite::arg_iterator ai=cs.arg_begin(); ai!=cs.arg_end(); ++ai)
        if(*ai == I) // but as an argument ==> escape
          return true;
    }
      break;
    default: // used in any other ways ==> escape
      return true;
    }
  }
  return false;
}

}
