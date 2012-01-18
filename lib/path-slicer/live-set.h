#ifndef __TERN_PATH_SLICER_LIVE_SET_H
#define __TERN_PATH_SLICER_LIVE_SET_H

#include <set>
#include <vector>
#include <ext/hash_set>

#include "llvm/ADT/DenseSet.h"
#include "bc2bdd/ext/JavaBDD/buddy/src/bdd.h"

#include "dyn-instrs.h"
#include "stat.h"
#include "macros.h"
#include "type-defs.h"

namespace tern {
  struct LiveSet {
  private:
    Stat *stat;

    /** Store all the virtual registers, they are only calling context plus the value pointer 
          of virtual registers. **/
    CtxVDenseSet virtRegs;

    /** Store all the load instructions. **/
    llvm::DenseSet<DynInstr *> loadInstrs;

    /** Store all the pointed to locations of pointer operands of all load instructions. **/
    bdd loadInstrsBDD;

  protected:

  public:
    LiveSet();
    ~LiveSet();
    size_t virtRegsSize();
    void clear();

    /** Virtual registers. **/
    void addReg(DynOprd *dynOprd);          /** Add a virtual register to live set. Skip all constants 
                                                                    such as @x and "1". Each register is a hash set of 
                                                                    <call context, static value pointer>. **/
    void delReg(DynOprd *dynOprd);
    bool regIn(DynOprd *dynOprd);
    CtxVDenseSet &getAllRegs();  /** Get the hash set of all virtual regs in live set. **/
    void addUsedRegs(DynInstr *dynInstr);    /** Add all the used operands of an instruction to live set 
                                                                          (include call args). It uses the addReg() above. **/
 
    /** Load memory locations. **/
    void addLoadMem(DynInstr *dynInstr);     /** Add a load memory location (the load instruction) to
                                                                          live set. The pointed-to locations of them are 
                                                                          represented as refcounted-bdd. **/
    void delLoadMem(DynInstr *dynInstr);     /** Delete a load memory location from the refcounted-bdd. **/
    llvm::DenseSet<DynInstr *> &getAllLoadInstrs();    /** Get all load instructions. This is used in handleMem(). **/
    const bdd getAllLoadMem();   /** Get the abstract locations for all load memory locations.
                                                This is used in writtenBetween() and mayWriteFunc(). **/

  };
}

#endif

