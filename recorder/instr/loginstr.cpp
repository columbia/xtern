#include "util.h"
#include "loggable.h"
#include "loginstr.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/TypeBuilder.h"

using namespace llvm;
using namespace std;

namespace tern {

char LogInstr::ID = 0;

void LogInstr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool LogInstr::runOnModule(Module &M) {
  const FunctionType *functype;

  targetData = getAnalysisIfAvailable<TargetData>();
  context = &getGlobalContext();

  addrType = Type::getInt8PtrTy(*context);
  dataType = Type::getInt64Ty(*context);

  functype = TypeBuilder<void (types::i<32>), false>::get(*context);
  logInsid = dyn_cast<Value>(M.getOrInsertFunction("tern_log_insid", functype));
  functype = TypeBuilder<void (types::i<32>, types::i<8>*,
                               types::i<64>), false>::get(*context);
  logLoad  = dyn_cast<Value>(M.getOrInsertFunction
                             ("tern_log_load", functype));
  logStore = dyn_cast<Value>(M.getOrInsertFunction
                             ("tern_log_store", functype));
  functype = TypeBuilder<void (types::i<32>, types::i<32>, types::i<8>*,
                               ...), false>::get(*context);
  logCall = dyn_cast<Value>(M.getOrInsertFunction("tern_log_call", functype));
  functype = TypeBuilder<void (types::i<32>, types::i<32>, types::i<8>*,
                             types::i<64>), false>::get(*context);
  logRet = dyn_cast<Value>(M.getOrInsertFunction("tern_log_ret", functype));

  forallfunc(M, fi) {
    // instr func if not logging its callsites
    if(!LoggableCallToFunc(fi))
      instrFunc(*fi);
  }
  return true;
}

void LogInstr::instrFunc(Function &F) {
  forall(Function, BB, F) {
    BasicBlock::iterator cur, prv;
    for(cur=BB->begin(); cur!=BB->end();) {
      prv = cur;
      cur ++;
      if(!Loggable(prv))
        continue;
      // mark instruction loggable
      SetLoggable(*context, prv);
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

Value *LogInstr::castIfNecessary(Value *srcval, const Type *dst,
                                 Instruction *insert) {
  const Type *src = srcval->getType();
  if(src == dst) return srcval;
  if(src->isPointerTy())
    return CastInst::CreatePointerCast(srcval, dst, "", insert);
  return CastInst::CreateZExtOrBitCast(srcval, dst, "", insert);
}

void LogInstr::instrLoadStore(Value *insid, Value *addr,
                              Value *data, Instruction *ins, bool isload) {
  BasicBlock::iterator ii = ins;
  Instruction *insert_ins = ++ii;  // can't be NULL; every BB has a terminator

  Value *logFunc = (isload? logLoad : logStore);

  addr = castIfNecessary(addr, addrType, insert_ins);
  data = castIfNecessary(data, dataType, insert_ins);

  vector<Value*> args;
  args.push_back(insid);
  args.push_back(addr);
  args.push_back(data);
  CallInst::Create(logFunc, args.begin(), args.end(), "", insert_ins);
}

void LogInstr::instrLoad(LoadInst *load) {
  Value *insid = GetInsID(load); assert(insid);
  Value *addr = load->getPointerOperand();
  Value *data = load;
  instrLoadStore(insid, addr, data, load, true);
}

void LogInstr::instrStore(StoreInst *store) {
  Value *insid = GetInsID(store); assert(insid);
  Value *addr = store->getPointerOperand();
  Value *data = store->getOperand(0);
  instrLoadStore(insid, addr, data, store, false);
}

void LogInstr::instrFirstNonPHI(Instruction *ins) {
  assert(ins == ins->getParent()->getFirstNonPHI());
  Value *insid = GetInsID(ins); assert(insid);

  vector<Value*> args;
  args.push_back(insid);
  CallInst::Create(logInsid, args.begin(), args.end(), "", ins);
}

void LogInstr::instrCall(CallInst *call) {
  CallSite cs(call);
  Value *insid = GetInsID(call); assert(insid);
  unsigned narg = cs.arg_end() - cs.arg_begin();
  Value *nargval = ConstantInt::get(Type::getInt32Ty(*context), narg);

  Value *func = cs.getCalledValue();
  func = castIfNecessary(func, addrType, call);

  // log call
  vector<Value*> args;
  args.push_back(insid);
  args.push_back(nargval);
  args.push_back(func);
  for(CallSite::arg_iterator ai=cs.arg_begin(); ai!=cs.arg_end(); ++ai) {
    Value *arg = ai->get();
    arg = castIfNecessary(arg, dataType, call);
    args.push_back(arg);
  }
  CallInst::Create(logCall, args.begin(), args.end(), "", call);

  // log return
  BasicBlock::iterator ii = call;
  Instruction *next_ins = ++ii;  // can't be NULL; every BB has a terminator
  Value *data = call;
  const Type *voidType = Type::getVoidTy(*context);
  if(data->getType() != voidType)
    data = castIfNecessary(data, dataType, next_ins);
  else
    data = Constant::getNullValue(dataType);
  args.clear();
  args.push_back(insid);
  args.push_back(nargval);
  args.push_back(func);
  args.push_back(data);
  CallInst::Create(logRet, args.begin(), args.end(), "", next_ins);
}

} // namespace tern

namespace {

static RegisterPass<tern::LogInstr>
X("loginstr", "instrumentation for recoding", false, false);

}
