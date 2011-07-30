/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "config.h"
#include "util.h"
#include "common/instr/instrutil.h"
#include "loggable.h"
#include "loginstr.h"
#include "recorder/runtime/logdefs.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/TypeBuilder.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

namespace tern {

char LogInstr::ID = 0;

void LogInstr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  ModulePass::getAnalysisUsage(AU);
}

bool LogInstr::runOnModule(Module &M) {

  const FunctionType *functype;

  // avoid instrument an already instrumented module
  assert(!M.getFunction("tern_log_load")
         && !M.getFunction("tern_log_store")
         && "LogInstr: instrument a already instrumented module!");

  targetData = getAnalysisIfAvailable<TargetData>();
  context = &getGlobalContext();

  addrType = Type::getInt8PtrTy(*context);
  dataType = Type::getInt64Ty(*context);

  getFuncs(M);
  markFuncs(M);
  exportFuncs();

  functype = TypeBuilder<void (types::i<32>), false>::get(*context);
  logInsid = dyn_cast<Value>(M.getOrInsertFunction("tern_log_insid", functype));
  functype = TypeBuilder<void (types::i<32>, types::i<8>*,
                               types::i<64>), false>::get(*context);
  logLoad  = dyn_cast<Value>(M.getOrInsertFunction
                             ("tern_log_load", functype));
  logStore = dyn_cast<Value>(M.getOrInsertFunction
                             ("tern_log_store", functype));
  functype = TypeBuilder<void (types::i<8>, types::i<32>, types::i<32>,
                               types::i<8>*, ...), false>::get(*context);
  logCall = dyn_cast<Value>(M.getOrInsertFunction("tern_log_call", functype));
  functype = TypeBuilder<void (types::i<8>, types::i<32>, types::i<32>,
                               types::i<8>*, types::i<64>),false>::get(*context);
  logRet = dyn_cast<Value>(M.getOrInsertFunction("tern_log_ret", functype));

  forallfunc(M, fi) {
    // instr function @fi if it is loggable
    if(funcBodyLogged(fi) == Logged)
      instrFunc(*fi);
  }

  return true;
}

void LogInstr::exportFuncs(void) {
  string ErrorInfo;
  raw_fd_ostream f(func_map_file.c_str(), ErrorInfo);
  assert(!f.has_error() && "can't open file for writing function name->id map!");
  forall(func_map_t, fi, funcsCallLogged) {
    bool escape = (funcsEscape.find(fi->first) != funcsEscape.end());
    f << fi->second << " " << (escape? 1 : 0)
      << " " << fi->first->getName()<< "\n";
  }
}

void LogInstr::getFuncs(Module &M) {
  unsigned funcid = Intrinsic::num_intrinsics;
  forallfunc(M, fi) {
    if(funcCallLogged(fi) == Logged) {
      func_map_t::iterator i = funcsCallLogged.find(fi);
      if(i == funcsCallLogged.end()) {
        ++ funcid;
        funcsCallLogged[fi] = funcid;
        if(funcEscapes(fi))
          funcsEscape[fi] = funcid;
      }
    }
  }
}

/// create function tern_all_loggable_callees() and mark loggable callees
void LogInstr::markFuncs(Module &M) {
  const char *markall_name = "tern_funcs_call_logged";
  const FunctionType *functype = TypeBuilder<void (void), false>::get(*context);

  Function *markall = dyn_cast<Function>
    (M.getOrInsertFunction(markall_name, functype));
  BasicBlock *entry = BasicBlock::Create(*context, "entry", markall);
  IRBuilder<> builder(entry);

  Function *markone;
  const char *markone_name;

  markone_name = "tern_func_call_logged";
  functype = TypeBuilder<void (types::i<8>*, types::i<32>),
    false>::get(*context);
  markone = dyn_cast<Function>
    (M.getOrInsertFunction(markone_name, functype));
  forall(func_map_t, fi, funcsCallLogged) {
    Value *args[2];
    args[0] = builder.CreateBitCast(fi->first, addrType, "tern_funcast");
    args[1] = ConstantInt::get(Type::getInt32Ty(*context), fi->second);
    builder.CreateCall2(markone, args[0], args[1]);
  }

  markone_name = "tern_func_escape";
  functype = TypeBuilder<void (types::i<8>*, types::i<32>),
    false>::get(*context);
  markone = dyn_cast<Function>
    (M.getOrInsertFunction(markone_name, functype));
  forall(func_map_t, fi, funcsEscape) {
    Value *args[2];
    args[0] = builder.CreateBitCast(fi->first, addrType, "tern_funcast");
    args[1] = ConstantInt::get(Type::getInt32Ty(*context), fi->second);
    builder.CreateCall2(markone, args[0], args[1]);
  }

  BasicBlock *ret = BasicBlock::Create(*context, "ret", markall);
  builder.CreateBr(ret);
  builder.SetInsertPoint(ret);
  builder.CreateRetVoid();
}

void LogInstr::instrInst(Instruction* I) {
  switch(I->getOpcode()) {
  case Instruction::Load:
    instrLoad(dyn_cast<LoadInst>(I));
    break;
  case Instruction::Store:
    instrStore(dyn_cast<StoreInst>(I));
    break;
  case Instruction::Call:
  case Instruction::Invoke:
    instrCall(I);
    break;
  default:
    assert(0 && "unknown instruction that must be logged!");
  }
}

void LogInstr::instrFunc(Function &F) {
  forall(Function, BB, F) {
    BasicBlock::iterator cur, prv;
    for(cur=BB->begin(); cur!=BB->end();) {
      prv = cur;
      cur ++;
      LogTag tag = instLogged(prv);
      switch(tag) {
      case NotLogged:
      case LogSync: // handled by SyncInstr
        break;
      case LogBBMarker: // just a marker
        instrBBMarker(prv);
        break;
      default: // must log the details of the inst
        instrInst(prv);
        break;
      } //switch(tag)
    } // for(cur...)
  } // forall(...)
}

Value *LogInstr::castIfNecessary(Value *srcval, const Type *dst,
                                 Instruction *insert) {
  const Type *src = srcval->getType();
  if(src == dst) return srcval;

  if(src->isPointerTy())
    //return CastInst::CreatePointerCast(srcval, dst, "tern_ptrcast", insert);
    return CastInst::CreatePointerCast(srcval, dst, "", insert);
  //return CastInst::CreateZExtOrBitCast(srcval, dst, "tern_valcast", insert);
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
  Value *insid = getInsID(load); assert(insid);
  Value *addr = load->getPointerOperand();
  Value *data = load;
  instrLoadStore(insid, addr, data, load, true);
}

void LogInstr::instrStore(StoreInst *store) {
  Value *insid = getInsID(store); assert(insid);
  Value *addr = store->getPointerOperand();
  Value *data = store->getOperand(0);
  instrLoadStore(insid, addr, data, store, false);
}

void LogInstr::instrBBMarker(Instruction *ins) {
  assert(ins == ins->getParent()->getFirstNonPHI());
  Value *insid = getInsID(ins); assert(insid);

  vector<Value*> args;
  args.push_back(insid);
  CallInst::Create(logInsid, args.begin(), args.end(), "", ins);
}

// FIXME1: InvokeInst may throw an exception, which we currently don't log
// FIXME2: InvokeInst target may have multiple precedessors; we handle
// only the case with a single predecessor (i.e., the current BB which has
// this InvokeInst)
void LogInstr::instrCall(Instruction *call) {
  CallSite cs(call);
  Value *insid = getInsID(call); assert(insid);
  unsigned narg = cs.arg_end() - cs.arg_begin();
  Value *nargval = ConstantInt::get(Type::getInt32Ty(*context), narg);

  Value *func = cs.getCalledValue();
  func = castIfNecessary(func, addrType, call);

  uint8_t flags = 0;
  Function *callee = cs.getCalledFunction();
  if(!callee)
    flags |=  CallIndirect;
  if(callee) {
    if(callee->doesNotReturn())
      flags |= CallNoReturn;
    if(funcsEscape.find(callee) != funcsEscape.end())
      flags |= CalleeEscape;
  }

  Value *flagsVal = ConstantInt::get(Type::getInt8Ty(*context), flags);

  assert((!callee || !callee->getName().startswith("tern"))
         && "instrumenting call to a tern function!");

  // log call
  vector<Value*> args;
  args.push_back(flagsVal);
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
  Instruction *next_ins;
  if(call->getOpcode() == Instruction::Call)
    next_ins = ++ii;  // can't be NULL; every BB has a terminator
  else {// invoke
    InvokeInst *invoke = dyn_cast<InvokeInst>(call);
    BasicBlock *normalDest = invoke->getNormalDest();
    assert(normalDest->getSinglePredecessor() == invoke->getParent()
           && "invoke normal desk has more than one predecessors"\
           " --- unsupported!");
    next_ins = normalDest->getFirstNonPHI();
  }
  Value *data = call;
  const Type *voidType = Type::getVoidTy(*context);
  if(data->getType() != voidType)
    data = castIfNecessary(data, dataType, next_ins);
  else
    data = Constant::getNullValue(dataType);
  args.clear();
  args.push_back(flagsVal);
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
