#ifndef __TERN_PATH_SLICER_LIVE_SET_H
#define __TERN_PATH_SLICER_LIVE_SET_H

#include <set>
#include <vector>

#include "llvm/ADT/DenseSet.h"
#include "bc2bdd/ext/JavaBDD/buddy/src/bdd.h"

#include "dyn-instrs.h"
#include "stat.h"
#include "macros.h"
#include "type-defs.h"
#include "alias-mgr.h"
#include "instr-id-mgr.h"
#include "func-summ.h"

namespace tern {  
  struct LiveSet {
  private:
    Stat *stat;
    AliasMgr *aliasMgr;
    InstrIdMgr *idMgr;
    FuncSumm *funcSumm;

    /** Store all the virtual registers, they are only calling context plus the value pointer 
          of virtual registers. **/
    CtxVDenseSet virtRegs;

    /** Store all the load instructions. **/
    llvm::DenseSet<DynInstr *> loadInstrs;
    llvm::DenseSet<DynInstr *> extCallLoadInstrs;
    bdd allLoadMem;
    bdd extCallLoadMem;

    /** Store all the pointed to locations of pointer operands of all load instructions. **/
    //RefCntBdd loadInstrsBdd;

  protected:
    void addReg(CallCtx *ctx, Value *v);

  public:
    LiveSet();
    ~LiveSet();
    void init(AliasMgr *aliasMgr, InstrIdMgr *idMgr, Stat *stat, FuncSumm *funcSumm);
    size_t virtRegsSize();
    size_t loadInstrsSize();
    void clear();

    /** Virtual registers. **/
    /** Add a virtual register to live set. Skip all constants such as @x and "1".
    Each register is a hash set of <call context, static value pointer>. **/
    void addReg(DynOprd *dynOprd);
    void delReg(DynOprd *dynOprd);
    bool regIn(DynOprd *dynOprd);

    /** Get the hash set of all virtual regs in live set. **/
    CtxVDenseSet &getAllRegs();  

    /** Add all the used operands of an instruction to live set (include call args).
    It uses the addReg() above. **/
    void addUsedRegs(DynInstr *dynInstr);    
 
    /** Load memory locations. **/
    /** Add a load memory location (the load instruction) to live set.
    The pointed-to locations of them are represented as refcounted-bdd. **/
    void addLoadMem(DynInstr *dynInstr);
    void addExtCallLoadMem(DynInstr *dynInstr);
                                                                          
    /** Delete a load memory location from the refcounted-bdd. **/
    void delLoadMem(DynInstr *dynInstr);     

    /** Get all load instructions. This is used in handleMem(). **/
    llvm::DenseSet<DynInstr *> &getAllLoadInstrs();    

    /** Get the abstract locations for all load memory locations.
    This is used in writtenBetween() and mayWriteFunc(). This bdd
    contains both real load instructions and sematic load from external calls. **/
    bdd getAllLoadMem(); 

    /** Get the abstract locations for all load memory locations.
    This is used in handleMem() in intra-thread phase. **/
    bdd getExtCallLoadMem();
  };
}

#endif

