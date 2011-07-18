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

#include "recorder/access/logaccess.h"

using namespace std;
using namespace llvm;
using namespace tern;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input log file>"),
    cl::init("-"), cl::value_desc("filename"));

static cl::opt<std::string>
BcFilename("bc", cl::desc("input .bc filename"),
           cl::value_desc(".bc filename"));

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv,
    "Print contents of a log file\n");

  auto_ptr<RawLog> log(new RawLog((InputFilename.c_str())));
  RawLog::iterator ri;

  for(ri=log->begin(); ri!=log->end(); ++ri) {
    PrintRecord(outs(), *ri) << "\n";
  }

  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  auto_ptr<Module> M;

  if(BcFilename.compare(BcFilename.length()-2, 3, ".ll"))
    M.reset(ParseAssemblyFile(BcFilename, Err, Context));
  else
    M.reset(ParseIRFile(BcFilename, Err, Context));

  if (M.get() == 0) {
    Err.Print(argv[0], errs());
    return 1;
  }

  PassManager Passes;
  IDManager   *IDM = new IDManager;

  Passes.add(new IDTagger);
  Passes.add(IDM);
  Passes.run(*M.get());

  InstLogBuilder b;
  auto_ptr<InstLog> instLog(b.create(log.get(), IDM));

  for(InstLog::InstVec::iterator i = instLog->instLog.begin();
      i != instLog->instLog.end(); ++i) {
    if(i->isLogged)
      outs() << "index=" << i->inst;
    else
      outs() << "insid=" << i->inst;
    outs() << "\n";
  }
}

