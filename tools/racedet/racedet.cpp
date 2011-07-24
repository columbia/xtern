/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/CommandLine.h"

#include "common/id-manager/IDTagger.h"
#include "common/id-manager/IDManager.h"
#include "common/instr/instrutil.h"
#include "recorder/access/logaccess.h"

using namespace std;
using namespace llvm;
using namespace tern;

static cl::opt<int>
NThreads(cl::Positional, cl::desc("<number of threads>"),
         cl::init(0), cl::value_desc("integer"));

static cl::opt<std::string>
BcFilename("bc", cl::desc("input .bc filename"),
           cl::value_desc(".bc filename"));

static cl::opt<std::string>
FuncsFilename("funcs", cl::desc("input .funcs filename, which maps function"\
                                " names to function IDs in the raw log"),
           cl::value_desc(".funcs filename"));

static bool endsWith(const string& str, const char* suffix) {
  size_t len = str.length();
  size_t suffix_len = strlen(suffix);
  return str.compare(len-suffix_len, suffix_len, suffix) == 0;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv,
      "Given a raw log file, print the instruction log"\
      " computed based on the raw log\n");

  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  auto_ptr<Module> M;

  if(endsWith(BcFilename, ".ll"))
    M.reset(ParseAssemblyFile(BcFilename, Err, Context));
  else
    M.reset(ParseIRFile(BcFilename, Err, Context));
  if (M.get() == 0) {
    Err.Print(argv[0], errs());
    return 1;
  }

  if(BcFilename.find("-record.bc") != BcFilename.npos
     || BcFilename.find("-record.ll") != BcFilename.npos)
    assert(0 && "record.bc specified; use analysis.bc instead!");

  const std::string &ModuleDataLayout = M.get()->getDataLayout();
  assert(!ModuleDataLayout.empty());

  PassManager Passes;
  TargetData  *TD  = new TargetData(ModuleDataLayout);
  IDManager   *IDM = new IDManager;
  Passes.add(TD);
  Passes.add(IDM);
  Passes.run(*M.get());

  if(FuncsFilename.empty()) { // set default .funcs file
    FuncsFilename = BcFilename;
    if(endsWith(FuncsFilename, ".ll")
       || endsWith(FuncsFilename, ".bc"))
      FuncsFilename = FuncsFilename.substr(0, FuncsFilename.length() - 3);
    if(endsWith(FuncsFilename, "-record")
      || endsWith(FuncsFilename, "-replay"))
      FuncsFilename = FuncsFilename.substr(0, FuncsFilename.length() - 7);
    else if(endsWith(FuncsFilename, "-analysis"))
      FuncsFilename = FuncsFilename.substr(0, FuncsFilename.length() - 9);
    FuncsFilename += ".funcs";
  }

  setTargetData(TD);
  setIDManager(IDM);
  InstLog::setFuncMap(FuncsFilename.c_str(), *M.get());

  ProgInstLog progLog;
  progLog.create(NThreads);
}
