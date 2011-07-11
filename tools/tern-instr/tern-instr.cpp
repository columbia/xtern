/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/PassNameParser.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

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
OutputFilename("o", cl::desc("Override output filename"),
               cl::value_desc("filename"), cl::init("-"));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

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

  string ErrorInfo;
  raw_ostream *Out = new raw_fd_ostream(OutputFilename.c_str(), ErrorInfo,
                                        raw_fd_ostream::F_Binary);

  PassManager Passes;
  const std::string &ModuleDataLayout = M.get()->getDataLayout();
  if (!ModuleDataLayout.empty())
    Passes.add(new TargetData(ModuleDataLayout));

  // TODO: all optimization passes

  Passes.add(new IDTagger);
  Passes.add(new SyncInstr);
  Passes.add(new LogInstr);
  Passes.add(new InitInstr);

  // output analysis.bc

  // clone module

  // replay.bc

  // record.bc

  // Check that the module is well formed on completion of optimization
  Passes.add(createVerifierPass());

  if (OutputAssembly)
    Passes.add(createPrintModulePass(Out));
  else
    Passes.add(createBitcodeWriterPass(*Out));
  Passes.run(*M.get());

  delete Out;
  return 0;
}
