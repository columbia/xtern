/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <stdint.h>
#include <dlfcn.h>
#include "syncfuncs.h"
#include "syncinstr.h"
#include "llvm/LLVMContext.h"
#include "llvm/System/Path.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "instrutil.h"

using namespace std;
using namespace llvm;

namespace tern {

char SyncInstr::ID = 0;

void SyncInstr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  ModulePass::getAnalysisUsage(AU);
}

// this function is purely for getting the path if we're compiled down to
// a .so; see getTemplate() below
static void foo(void) {}

Module *SyncInstr::getTemplateModule(void) {
  MemoryBuffer *file;
  const char* templatebc = "template.bc";

  // 1st try: current directory
  file = MemoryBuffer::getFile(templatebc);

  // 2nd try: executable directory
  if(!file) {
    // this reads /proc/self/exe on linux
    sys::Path prog = sys::Path::GetMainExecutable(NULL, NULL);
    prog.eraseComponent();
    prog.eraseComponent();
    if(!prog.isEmpty()) {
      prog.appendComponent("lib");
      prog.appendComponent(templatebc);
      file = MemoryBuffer::getFile(prog.c_str());
    }
  }

  // 3rd try: search the directory of .so if we are compiled down to one
  if(!file) {
    Dl_info info;
    // direct cast to void* triggers a g++ warning
    intptr_t addr = (intptr_t)foo;
    if(dladdr((void*)addr, &info) < 0) {
      perror("dladdr");
    } else {
      sys::Path prog(info.dli_fname);
      prog.eraseComponent();
      if(!prog.isEmpty()) {
        prog.appendComponent(templatebc);
        file = MemoryBuffer::getFile(prog.c_str());
      }
    }
  }
  assert(file && "cannot find tempalte.bc!"                             \
         " make sure it is in the same path as the executable ");
  Module *tmp = ParseBitcodeFile(file, getGlobalContext());
  assert(tmp && "cannot parse template.bc!");

  return tmp;
}

void SyncInstr::initFuncTypes(Module &M) {
  Module *tmp = getTemplateModule();
  forallfunc(*tmp, fi) {
    if(fi->isDeclaration()) {
      unsigned syncid = syncfunc::getTernNameID(fi->getNameStr().c_str());
      // only care about sync functions not provided by us
      if(syncid != syncfunc::not_sync && !syncfunc::isTern(syncid))
        functypes[syncid] = fi->getFunctionType();
    }
  }
  for(unsigned i=0; i<syncfunc::num_syncs; ++i) {
    if(!syncfunc::isTern(i)) {
      assert(functypes.find(i) != functypes.end()
             && "can't find type for sync function!");
    }
  }
  delete tmp;
}

void SyncInstr::warnEscapeFuncs(Module &M) {
  forallfunc(M, fi) {
    if(fi->isDeclaration()) {
      unsigned syncid = syncfunc::getNameID(fi->getNameStr().c_str());
      if(syncid != syncfunc::not_sync && !syncfunc::isTern(syncid)) {
        if(funcEscapes(fi))
          errs()<< "Function " << fi->getName() << " escapes!";
      }
    }
  }
}

void SyncInstr::replaceFunctionInCall(Module &M, Instruction *I, unsigned syncid) {

  CallSite cs(I);
  InvokeInst *IV;
  Instruction *NI = NULL;

  // find hook function
  const FunctionType *functype = functypes[syncid]; assert(functype);
  const char *funcname = syncfunc::getTernName(syncid);
  Constant *C = M.getOrInsertFunction(funcname, functype);
  assert (C && "cannot create hook function!");
  Function *hook = dyn_cast<Function>(C);
  assert(hook && "cannot create hook function!");

  // set up arguments
  SmallVector<Value*, 8> args;
  Value *insid = getInsID(I);
  assert(insid && "must run IDTagger before SyncInstr!");
  args.push_back(insid);
  for(CallSite::arg_iterator ai=cs.arg_begin(),ae=cs.arg_end(); ai!=ae; ++ai) {
    assert(*ai);
    args.push_back(*ai);
  }

  switch(I->getOpcode()) {
  case Instruction::Call:
    NI = CallInst::Create(hook, args.begin(), args.end(), "");
    break;
  case Instruction::Invoke:
    IV = dyn_cast<InvokeInst>(I);
    NI = InvokeInst::Create(hook, IV->getNormalDest(), IV->getUnwindDest(),
                            args.begin(), args.end(), "");
  default:
    abort();
  }

  // copy metadata since ReplaceInstWithInsit doesn't copy for us
  SmallVector<std::pair<unsigned, MDNode*>, 8> MDs;
  SmallVector<std::pair<unsigned, MDNode*>, 8>::iterator MI, ME;

  I->getAllMetadata(MDs);
  for(MI=MDs.begin(), ME=MDs.end(); MI!=ME; ++MI)
    NI->setMetadata(MI->first, MI->second);

  // this will delete I
  ReplaceInstWithInst(I, NI);

}

bool SyncInstr::runOnModule(Module &M) {

  initFuncTypes(M);

  warnEscapeFuncs(M);

  // replace calls to sync functions with calls to tern hooks
  forallbb(M, BB) {
    BasicBlock::iterator prv, cur;
    for(cur=BB->begin(); cur!=BB->end();) {
      prv = cur;
      cur ++;

      Function *F = NULL;
      switch(prv->getOpcode()) {
      case Instruction::Call:
        F = dyn_cast<CallInst>(prv)->getCalledFunction();
        break;
      case Instruction::Invoke:
        F = dyn_cast<InvokeInst>(prv)->getCalledFunction();
        break;
      }
      if(!F) continue;

      const char* name = F->getNameStr().c_str();
      unsigned syncid = syncfunc::getNameID(name);
      if(syncid == syncfunc::not_sync)
        continue;
      replaceFunctionInCall(M, prv, syncid);
    }
  }

  return true;
}

} // namespace tern

namespace {

static RegisterPass<tern::SyncInstr>
X("syncinstr", "instrumentation of synchronization methods", false, false);

}