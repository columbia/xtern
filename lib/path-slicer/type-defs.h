#ifndef __TERN_PATH_SLICER_TYPE_DEFS_H
#define __TERN_PATH_SLICER_TYPE_DEFS_H

#include <vector>
#include "llvm/Value.h"

typedef std::pair<void *, void *> PtrPair;
typedef std::pair<PtrPair, PtrPair> PtrPairPair;
typedef std::pair< std::vector<int> *, llvm::Value * > CtxVPair;

typedef llvm::DenseSet< CtxVPair > CtxVDenseSet;
typedef std::vector<int> CallCtx;
typedef llvm::DenseSet<llvm::Instruction * > InstrDenseSet;

typedef unsigned char uchar;

#endif
