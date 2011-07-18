/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"
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
OutputFilename("o", cl::desc("Output filename prefix (without .bc or .ll)"),
               cl::value_desc("filename"));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

static bool endsWith(const string& str, const char* suffix) {
  size_t len = str.length();
  size_t suffix_len = strlen(suffix);
  return str.compare(len-suffix_len, suffix_len, suffix) == 0;
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
  PassManager Passes1, Passes2, Passes3;

  // TODO: all optimization passes

  const std::string &ModuleDataLayout = M.get()->getDataLayout();

  // analysis instrumentation
  if (!ModuleDataLayout.empty())
    Passes1.add(new TargetData(ModuleDataLayout));
  Passes1.add(new IDTagger);
  // Check that the module is well formed on completion of optimization
  Passes1.add(createVerifierPass());
  if (OutputAssembly)
    Passes1.add(createPrintModulePass(AnalysisOut));
  else
    Passes1.add(createBitcodeWriterPass(*AnalysisOut));

  // record instrumentation
  if (!ModuleDataLayout.empty())
    Passes2.add(new TargetData(ModuleDataLayout));
  Passes2.add(new SyncInstr);
  Passes2.add(createVerifierPass());
  Passes2.add(pass = new LogInstr); pass->setFuncsExportFile(FuncMapFilename);
  Passes2.add(createVerifierPass());
  Passes2.add(new InitInstr);
  // Check that the module is well formed on completion of optimization
  Passes2.add(createVerifierPass());
  if (OutputAssembly)
    Passes2.add(createPrintModulePass(RecordOut));
  else
    Passes2.add(createBitcodeWriterPass(*RecordOut));

  // replay instrumentation
  if (!ModuleDataLayout.empty())
    Passes3.add(new TargetData(ModuleDataLayout));
  // TODO add loom.bit
  // Check that the module is well formed on completion of optimization
  Passes3.add(createVerifierPass());
  if (OutputAssembly)
    Passes3.add(createPrintModulePass(ReplayOut));
  else
    Passes3.add(createBitcodeWriterPass(*ReplayOut));

  Passes1.run(*M.get());
  auto_ptr<Module> MC;
  MC.reset(CloneModule(M.get()));
  Passes2.run(*M.get());
  Passes3.run(*MC.get());

  delete AnalysisOut;
  delete RecordOut;
  delete ReplayOut;

  return 0;
}
