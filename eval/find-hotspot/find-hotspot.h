#ifndef __FIND_HOTSPOT_H
#define __FIND_HOTSPOT_H

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Constant.h"
#include "llvm/ADT/SmallVector.h"


#include <set>

namespace tern {
  struct FindHotspot: public llvm::ModulePass {
  //typedef llvm::SmallVectorImpl<std::pair<const llvm::BasicBlock*,const llvm::BasicBlock*>, 32> BackEdgeVector;
  private:
    /*  void backedge_stat(int id); */
    const llvm::Type *int_type;
    llvm::Constant *backedge_stat;

  protected:
    void addBackEdgeStat(llvm::SmallVector<std::pair<const llvm::BasicBlock*,const llvm::BasicBlock*>, 32> &edges);
    
  public:
    static char ID;
    FindHotspot();
    virtual ~FindHotspot();
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const;
    void init(llvm::Module &M);
  };
}

#endif
