#include "util.h"
#include "recinstr.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/TypeBuilder.h"

using namespace llvm;
using namespace std;

namespace tern 
{
  char RecInstr::ID = 0;

  void RecInstr::getAnalysisUsage(AnalysisUsage &AU) const 
  {
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }
  
  bool RecInstr::runOnModule(Module &M) 
  {

    targetData = getAnalysisIfAvailable<TargetData>();
    context = &getGlobalContext();

    addrType = Type::getInt8PtrTy(*context);

    for(unsigned i=0; i<ARRAY_LEN(dataTypes); ++i) {
      char name[32];
      int width = 8*(1<<i);
      const char* logfunc = "tern_log_i";
      const FunctionType *functype = getFuncTy(width);

      dataTypes[i] = getIntTy(width);
      sprintf(name, "%s%d", logfunc, width);
      logFuncs[i] = M.getOrInsertFunction(name, functype);
    }

    forallfunc(M, fi) {
      //if(summaries.get(fi->getName()))
      //  continue;
      // errs() << "instrFunc " << fi->getName() << "\n";
      instrFunc(*fi);
    }

    return true;
  }

  void RecInstr::instrFunc(Function &F) 
  {
    forall(Function, BB, F) {
      BasicBlock::iterator cur, prv;
      bool hasInstr = false;
      for(cur=BB->begin(); cur!=BB->end();) {
        prv = cur;
        cur ++;
        switch(prv->getOpcode()) {
        case Instruction::Load:
          hasInstr |= instrLoad(dyn_cast<LoadInst>(prv));
          break;
        case Instruction::Store:
          hasInstr |= instrStore(dyn_cast<StoreInst>(prv));
          break;
        case Instruction::Call:
          hasInstr |= instrCall(dyn_cast<CallInst>(prv));
          break;
        case Instruction::Invoke:
          hasInstr |= instrInvoke(dyn_cast<InvokeInst>(prv));
          break;
        default:
          break;
        }
        // if no instruction recorded in this BB, record_bb
      }
    }
  }

  void RecInstr::instrLoadOrStore(Value *insid, Value *addr, 
                                  Value *data, Instruction *ins) 
  {
    unsigned index;
    unsigned w = targetData->getTypeStoreSize(data->getType());
    assert(w <= (1<<MaxTypeNum));
    for(index=0; (1u<<index) < w; ++index)
      ;

    Instruction *next_ins;
    BasicBlock::iterator ii = ins;
    next_ins = ++ii;

    if(addr->getType() != addrType)
      addr = new BitCastInst(addr, addrType, "", next_ins);
    if(data->getType() != dataTypes[index])
      data = new BitCastInst(ins, dataTypes[index], "", next_ins);
    
    vector<Value*> args;
    args.push_back(insid);
    args.push_back(addr);
    args.push_back(data);
    Value *func = (Value*)(logFuncs[index]);
    CallInst::Create(func, args.begin(), args.end(), "", next_ins);
  }

  bool RecInstr::instrLoad(LoadInst *load) 
  {
    Value *insid = getInsID(load);
    Value *addr = load->getPointerOperand();
    Value *data = load;

    if(!insid) return false;

    instrLoadOrStore(insid, addr, data, load);
    return true;
  }

  bool RecInstr::instrStore(StoreInst *store) 
  {
    Value *insid = getInsID(store);
    Value *addr = store->getPointerOperand();
    Value *data = store->getOperand(0);

    if(!insid) return false;

    instrLoadOrStore(insid, addr, data, store);
    return true;
  }



  bool RecInstr::instrCall(CallInst *call) 
  {
    // record_load
    // call
    // record_store
    // record_ret

    //if(!summaries.get(fi->getName()))
    //  return false;

    // if external w/o summary
    //   conservatively assume read/write all pointers (what about length?)

    // store function type in function metadata
    //   app
    //   lib
    //   tern

    //  summarized
    return true;
  }

  const IntegerType* RecInstr::getIntTy(unsigned width)
  {
    const IntegerType *t;
    switch(width) {
    case  8: t = Type::getInt8Ty(*context);  break;
    case 16: t = Type::getInt16Ty(*context); break;
    case 32: t = Type::getInt32Ty(*context); break;
    case 64: t = Type::getInt64Ty(*context); break;
    default: assert_not_supported();
    }
    return t;
  }
  
  const FunctionType* RecInstr::getFuncTy(unsigned width)
  {
    const FunctionType *t;
    switch(width) {
    case  8: t = TypeBuilder<void (types::i<32>, types::i<8>*, 
                   types::i<8>), false>::get(*context);  break;
    case 16: t = TypeBuilder<void (types::i<32>, types::i<8>*, 
                   types::i<16>), false>::get(*context); break;
    case 32: t = TypeBuilder<void (types::i<32>, types::i<8>*, 
                   types::i<32>), false>::get(*context); break;
    case 64: t = TypeBuilder<void (types::i<32>, types::i<8>*, 
                   types::i<64>), false>::get(*context); break;
    default: assert_not_supported();
    }
    return t;
  }

  Value* RecInstr::getInsID(const Instruction *I) const 
  {
    MDNode *Node = I->getMetadata("ins_id");
    if (!Node)
      return NULL;
    assert(Node->getNumOperands() == 1);
    ConstantInt *CI = dyn_cast<ConstantInt>(Node->getOperand(0));
    assert(CI);
    return CI;
  }
}

namespace 
{
  static RegisterPass<tern::RecInstr> 
  X("recinstr", "instrumentation for recoding", false, false);
}
