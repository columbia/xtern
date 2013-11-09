/* Authors: Heming Cui (heming@cs.columbia.edu), Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_INSTRUTIL_H
#define __TERN_COMMON_INSTRUTIL_H

#include <stddef.h>
#include "llvm/Instruction.h"
#include "llvm/Target/TargetData.h"
#include "common/IDManager.h"

namespace tern {

llvm::Value* getIntMetadata(const llvm::Instruction *I, const char* key);
llvm::Value* getInsID(const llvm::Instruction *I);
bool funcEscapes(llvm::Function* F);
llvm::IDManager *getIDManager(void);
void setIDManager(llvm::IDManager * IDM);
llvm::TargetData *getTargetData(void);
void setTargetData(llvm::TargetData *td);


}

#endif
