#include "instrutil.h"
#include "llvm/Metadata.h"
#include "llvm/Constants.h"

using namespace llvm;

namespace tern {

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

}
