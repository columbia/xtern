/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "llvm/Linker.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/System/Path.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PassNameParser.h"

#include "common/id-manager/IDTagger.h"

#include "common/instr/initinstr.h"
#include "common/instr/syncinstr.h"
#include "recorder/instr/loginstr.h"

using namespace std;
using namespace llvm;
using namespace tern;

static cl::list<const PassInfo*, bool, PassNameParser>
PassList(cl::desc("Passes available:"));

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
    cl::init("-"), cl::value_desc("filename"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename prefix (without .bc or .ll)"),
               cl::value_desc("filename"));

static cl::opt<std::string>
Uclibc("uclibc", cl::desc("path to klee uclibc library)"),
       cl::value_desc("filename"));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

static bool endsWith(const string& str, const char* suffix) {
  size_t len = str.length();
  size_t suffix_len = strlen(suffix);
  if(len < suffix_len)
    return false;
  return str.compare(len-suffix_len, suffix_len, suffix) == 0;
}


// copied from klee
Module *linkWithLibrary(Module *module,
                        const std::string &libraryName) {
  Linker linker("klee", module, false);

  llvm::sys::Path libraryPath(libraryName);
  bool native = false;

  if (linker.LinkInFile(libraryPath, native)) {
    assert(0 && "linking in library failed!");
  }

  return linker.releaseModule();
}


static llvm::Module *linkWithUclibc(llvm::Module *mainModule) {
  Function *f;
  // force import of __uClibc_main
  mainModule->getOrInsertFunction("__uClibc_main",
        FunctionType::get(Type::getVoidTy(getGlobalContext()),
        std::vector<const Type*>(), true));

  f = mainModule->getFunction("__ctype_get_mb_cur_max");
  if (f) f->setName("_stdlib_mb_cur_max");

  // Strip of asm prefixes for 64 bit versions because they are not
  // present in uclibc and we want to make sure stuff will get
  // linked. In the off chance that both prefixed and unprefixed
  // versions are present in the module, make sure we don't create a
  // naming conflict.
  for (Module::iterator fi = mainModule->begin(), fe = mainModule->end();
       fi != fe;) {
    Function *f = fi;
    ++fi;
    const std::string &name = f->getName();
    if (name[0]=='\01') {
      unsigned size = name.size();
      if (name[size-2]=='6' && name[size-1]=='4') {
        std::string unprefixed = name.substr(1);

        // See if the unprefixed version exists.
        if (Function *f2 = mainModule->getFunction(unprefixed)) {
          f->replaceAllUsesWith(f2);
          f->eraseFromParent();
        } else {
          f->setName(unprefixed);
        }
      }
    }
  }

  mainModule = linkWithLibrary(mainModule, Uclibc);
  assert(mainModule && "unable to link with uclibc");

  // more sighs, this is horrible but just a temp hack
  //    f = mainModule->getFunction("__fputc_unlocked");
  //    if (f) f->setName("fputc_unlocked");
  //    f = mainModule->getFunction("__fgetc_unlocked");
  //    if (f) f->setName("fgetc_unlocked");

  Function *f2;
  f = mainModule->getFunction("open");
  f2 = mainModule->getFunction("__libc_open");
  if (f2) {
    if (f) {
      f2->replaceAllUsesWith(f);
      f2->eraseFromParent();
    } else {
      f2->setName("open");
      assert(f2->getName() == "open");
    }
  }

  f = mainModule->getFunction("fcntl");
  f2 = mainModule->getFunction("__libc_fcntl");
  if (f2) {
    if (f) {
      f2->replaceAllUsesWith(f);
      f2->eraseFromParent();
    } else {
      f2->setName("fcntl");
      assert(f2->getName() == "fcntl");
    }
  }

  // XXX we need to rearchitect so this can also be used with
  // programs externally linked with uclibc.

  // We now need to swap things so that __uClibc_main is the entry
  // point, in such a way that the arguments are passed to
  // __uClibc_main correctly. We do this by renaming the user main
  // and generating a stub function to call __uClibc_main. There is
  // also an implicit cooperation in that runFunctionAsMain sets up
  // the environment arguments to what uclibc expects (following
  // argv), since it does not explicitly take an envp argument.
  Function *userMainFn = mainModule->getFunction("main");
  assert(userMainFn && "unable to get user main");
  Function *uclibcMainFn = mainModule->getFunction("__uClibc_main");
  assert(uclibcMainFn && "unable to get uclibc main");
  userMainFn->setName("__user_main");

  const FunctionType *ft = uclibcMainFn->getFunctionType();
  assert(ft->getNumParams() == 7);

  std::vector<const Type*> fArgs;
  fArgs.push_back(ft->getParamType(1)); // argc
  fArgs.push_back(ft->getParamType(2)); // argv
  Function *stub = Function::Create(FunctionType::get(Type::getInt32Ty
      (getGlobalContext()), fArgs, false), GlobalVariable::ExternalLinkage,
                                    "main", mainModule);
  BasicBlock *bb = BasicBlock::Create(getGlobalContext(), "entry", stub);

  std::vector<llvm::Value*> args;
  args.push_back(llvm::ConstantExpr::getBitCast(userMainFn,
                                                ft->getParamType(0)));
  args.push_back(stub->arg_begin()); // argc
  args.push_back(++stub->arg_begin()); // argv
  args.push_back(Constant::getNullValue(ft->getParamType(3))); // app_init
  args.push_back(Constant::getNullValue(ft->getParamType(4))); // app_fini
  args.push_back(Constant::getNullValue(ft->getParamType(5))); // rtld_fini
  args.push_back(Constant::getNullValue(ft->getParamType(6))); // stack_end
  CallInst::Create(uclibcMainFn, args.begin(), args.end(), "", bb);

  new UnreachableInst(getGlobalContext(), bb);

  return mainModule;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv,
    "Prepare all .bc files (-record.bc, -analysis.bc, and -replay.bc)\n");

  // Load the input module
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));
  if (M.get() == 0) {
    Err.Print(argv[0], errs());
    return 1;
  }

  if(endsWith(OutputFilename, ".ll") || endsWith(OutputFilename, ".bc")) {
    errs() << "Option -o specifies output filename prefix "\
      "without .bc or .ll extension ";
    return 1;
  }

  string ext = (OutputAssembly? ".ll" : ".bc");
  string FuncMapFilename  = OutputFilename + ".funcs";
  string AnalysisFilename = OutputFilename + "-analysis" + ext;
  string RecordFilename   = OutputFilename + "-record"   + ext;
  string ReplayFilename   = OutputFilename + "-replay"   + ext;

  string ErrorInfo;
  raw_ostream *AnalysisOut = new raw_fd_ostream(AnalysisFilename.c_str(),
                                   ErrorInfo,  raw_fd_ostream::F_Binary);
  raw_ostream *RecordOut   = new raw_fd_ostream(RecordFilename.c_str(),
                                   ErrorInfo,  raw_fd_ostream::F_Binary);
  raw_ostream *ReplayOut   = new raw_fd_ostream(ReplayFilename.c_str(),
                                   ErrorInfo,  raw_fd_ostream::F_Binary);
  assert(AnalysisOut && "can't open output file!");
  assert(RecordOut   && "can't open output file!");
  assert(ReplayOut   && "can't open output file!");

  LogInstr *pass;
  PassManager Passes1, Passes2, Passes2init, Passes3;

  // TODO: all optimization passes

  const std::string &ModuleDataLayout = M.get()->getDataLayout();

  // analysis instrumentation
  if (!ModuleDataLayout.empty())
    Passes1.add(new TargetData(ModuleDataLayout));
  Passes1.add(new IDTagger);
  Passes1.add(new SyncInstr);
  Passes1.add(createVerifierPass());
  if (OutputAssembly)
    Passes1.add(createPrintModulePass(AnalysisOut));
  else
    Passes1.add(createBitcodeWriterPass(*AnalysisOut));

  // record instrumentation
  Passes2init.add(new InitInstr);
  if (!ModuleDataLayout.empty())
    Passes2.add(new TargetData(ModuleDataLayout));
Passes2.add(createVerifierPass());
  Passes2.add(pass = new LogInstr); pass->setFuncsExportFile(FuncMapFilename);
Passes2.add(createVerifierPass());
  Passes2.add(createVerifierPass());
  if (OutputAssembly)
    Passes2.add(createPrintModulePass(RecordOut));
  else
    Passes2.add(createBitcodeWriterPass(*RecordOut));

  // replay instrumentation
  if (!ModuleDataLayout.empty())
    Passes3.add(new TargetData(ModuleDataLayout));
  // TODO add loom.bit
  Passes3.add(createVerifierPass());
  if (OutputAssembly)
    Passes3.add(createPrintModulePass(ReplayOut));
  else
    Passes3.add(createBitcodeWriterPass(*ReplayOut));

  // analysis.bc
  Passes1.run(*M.get());

  // clone module for replay.bc
  auto_ptr<Module> MC;
  MC.reset(CloneModule(M.get()));

  // record.bc
  Passes2init.run(*M.get());
  if(!Uclibc.empty())
    M.reset(linkWithUclibc(M.get()));
  Passes2.run(*M.get());

  // replay.bc
  Passes3.run(*MC.get());

  delete AnalysisOut;
  delete RecordOut;
  delete ReplayOut;

  return 0;
}
