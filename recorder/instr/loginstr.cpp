#include "util.h"
#include "loggable.h"
#include "loginstr.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/TypeBuilder.h"

using namespace llvm;
using namespace std;

namespace tern 
{
  char LogInstr::ID = 0;

  void LogInstr::getAnalysisUsage(AnalysisUsage &AU) const 
  {
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }
  
  bool LogInstr::runOnModule(Module &M) 
  {

    targetData = getAnalysisIfAvailable<TargetData>();
    context = &getGlobalContext();

    addrType = Type::getInt8PtrTy(*context);
    dataType = Type::getInt64Ty(*context);
    const FunctionType *functype = 
      TypeBuilder<void (types::i<32>, types::i<8>*, 
                        types::i<64>), false>::get(*context);
    logFunc = M.getOrInsertFunction("tern_log", functype);

    forallfunc(M, fi) {
      // 
      // functions with body: app, lib, tern
      // external functions without body
      // 
      // if summary exists, use
      // else 
      //   use conservative summary for ext
      //   instr app and lib
      //
      //if(summaries.get(fi->getName()))
      //  continue;
      //errs() << "instrFunc " << fi->getName() << "\n";
      instrFunc(*fi);
    }

    return true;
  }

  void LogInstr::instrFunc(Function &F) 
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
      }
      // if no instruction logged in this BB, add a log record for this BB
      // so that we know it is executed
      if(!hasInstr) instrBB(BB);
    }
  }

  void LogInstr::instrInstruction(Value *insid, Value *addr, 
                                  Value *data, Instruction *ins, bool after) 
  {
    Instruction *insert_ins = ins;

    if(after) {
      BasicBlock::iterator ii = ins;
      insert_ins = ++ii;  // can't be NULL; every BB has a terminator
    }

    if(addr->getType() != addrType)
      addr = CastInst::CreateZExtOrBitCast(addr, addrType, "", insert_ins);
    if(data->getType() != dataType)
      data = CastInst::CreateZExtOrBitCast(data, dataType, "", insert_ins);
    
    vector<Value*> args;
    args.push_back(insid);
    args.push_back(addr);
    args.push_back(data);
    Value *func = (Value*)(logFunc);
    CallInst::Create(func, args.begin(), args.end(), "", insert_ins);
  }

  bool LogInstr::instrLoad(LoadInst *load) 
  {
    Value *insid = getInsID(load);
    Value *addr = load->getPointerOperand();
    Value *data = load;

    if(!insid) return false;

    instrInstruction(insid, addr, data, load);
    return true;
  }

  bool LogInstr::instrStore(StoreInst *store) 
  {
    Value *insid = getInsID(store);
    Value *addr = store->getPointerOperand();
    Value *data = store->getOperand(0);

    if(!insid) return false;

    instrInstruction(insid, addr, data, store);
    return true;
  }

  bool LogInstr::instrBB(BasicBlock *BB)
  {
    Instruction *ins = BB->getFirstNonPHI();
    Value *insid = getInsID(ins);
    Value *addr = Constant::getNullValue(Type::getInt8PtrTy(*context));
    Value *data = Constant::getNullValue(Type::getInt64Ty(*context));

    if(!insid) return false;

    instrInstruction(insid, addr, data, ins, false);
    return true;
  }

  bool LogInstr::instrCall(CallInst *call) 
  {
    // call
    // record_load
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

}

namespace 
{
  static RegisterPass<tern::LogInstr> 
  X("loginstr", "instrumentation for recoding", false, false);
}
