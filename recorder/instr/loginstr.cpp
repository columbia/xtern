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
    logFunc = dyn_cast<Value>(M.getOrInsertFunction("tern_log", functype));

    forallfunc(M, fi) {
      if(LoggableFunc(fi))
        instrFunc(*fi);
    }
    return true;
  }

  void LogInstr::instrFunc(Function &F) 
  {
    forall(Function, BB, F) {
      BasicBlock::iterator cur, prv;
      for(cur=BB->begin(); cur!=BB->end();) {
        prv = cur;
        cur ++;

        if(!Loggable(prv))
          continue;

        // mark instruction loggable
        SetLoggable(prv);
        switch(prv->getOpcode()) {
        case Instruction::Load:
          instrLoad(dyn_cast<LoadInst>(prv));
          break;
        case Instruction::Store:
          instrStore(dyn_cast<StoreInst>(prv));
          break;
        case Instruction::Call:
          instrCall(dyn_cast<CallInst>(prv));
          break;
        case Instruction::Invoke:
          instrInvoke(dyn_cast<InvokeInst>(prv));
          break;
        default: // no other ins loggable, log FirstNonPHI
          instrFirstNonPHI(prv);
        }
      }
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

  void LogInstr::instrLoad(LoadInst *load) 
  {
    Value *insid = GetInsID(load); assert(insid);
    Value *addr = load->getPointerOperand();
    Value *data = load;
    instrInstruction(insid, addr, data, load);
  }

  void LogInstr::instrStore(StoreInst *store) 
  {
    Value *insid = GetInsID(store); assert(insid);
    Value *addr = store->getPointerOperand();
    Value *data = store->getOperand(0);
    instrInstruction(insid, addr, data, store);
  }

  void LogInstr::instrFirstNonPHI(Instruction *ins)
  {
    assert(ins == ins->getParent()->getFirstNonPHI());
    Value *insid = GetInsID(ins); assert(insid);
    Value *addr = Constant::getNullValue(Type::getInt8PtrTy(*context));
    Value *data = Constant::getNullValue(Type::getInt64Ty(*context));
    instrInstruction(insid, addr, data, ins, false);
  }

  void LogInstr::instrCall(CallInst *call) 
  {
    CallSite cs(call);
    Value *insid = GetInsID(call); assert(insid);
    unsigned narg = cs.arg_end() - cs.arg_begin();
    Value *nargval = ConstantInt::get(Type::getInt32Ty(*context), narg);

    Value *func = cs.getCalledValue();
    if(func->getType() != addrType)
      func = CastInst::CreateZExtOrBitCast(func, addrType, "", call);

    // log call
    vector<Value*> args;
    args.push_back(insid);
    args.push_back(nargval);
    args.push_back(func);
    for(CallSite::arg_iterator ai=cs.arg_begin(); ai!=cs.arg_end(); ++ai) {
      Value *arg = ai->get();
      if(arg->getType() != dataType)
        arg = CastInst::CreateZExtOrBitCast(arg, dataType, "", call);
      args.push_back(arg);
    }
    CallInst::Create(logCallFunc, args.begin(), args.end(), "", call);

    // log return
    BasicBlock::iterator ii = call;
    Instruction *next_ins = ++ii;  // can't be NULL; every BB has a terminator
    Value *data = call;
    if(data->getType() != dataType)
      data = CastInst::CreateZExtOrBitCast(data, dataType, "", next_ins);

    args.clear();
    args.push_back(insid);
    args.push_back(nargval);
    args.push_back(func);
    args.push_back(data);
    CallInst::Create(logRetFunc, args.begin(), args.end(), "", call);
  }

}

namespace 
{
  static RegisterPass<tern::LogInstr> 
  X("loginstr", "instrumentation for recoding", false, false);
}
