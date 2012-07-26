#include "util.h"
#include "event-mgr.h"
#include "tern/syncfuncs.h"
using namespace tern;

#include "common/util.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

using namespace klee;

using namespace std;

char tern::EventMgr::ID = 0;
extern cl::opt<std::string> UseOneChecker;

namespace {
  static RegisterPass<tern::EventMgr> X(
    "event-manager",
    "Get functions with event operations",
    false,
    true); // is analysis
}

EventMgr::EventMgr(): ModulePass(&ID) {
  CG = NULL;
  checker = NULL;
  fprintf(stderr, "EventMgr::EventMgr UseOneChecker (%s)\n", UseOneChecker.c_str());
}


EventMgr::~EventMgr() {
  if (DBG)
    fprintf(stderr, "EventMgr::~EventMgr\n");
}

void EventMgr::initCallGraph(llvm::CallGraphFP *CG) {
  this->CG = CG;
}

void EventMgr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

void EventMgr::setupEvents(Module &M) {
  if (UseOneChecker == "") {
    fprintf(stderr, "EventMgr::isEventFunc checker is NULL, please use only one checker.\n");
    exit(1);
  } else {
    if (UseOneChecker == "Assert") {
      checker = new AssertChecker(NULL);
      errs() << "EventMgr::setupEvents::AssertChecker\n";
    } else if (UseOneChecker == "OpenClose") {
      checker = new OpenCloseChecker(NULL);
      errs() << "EventMgr::setupEvents::OpenCloseChecker\n";
    } else if (UseOneChecker == "Lock") {
      checker = new LockChecker(NULL);
      errs() << "EventMgr::setupEvents::LockChecker\n";
    } else if (UseOneChecker == "File") {
      checker = new FileChecker(NULL);
      errs() << "EventMgr::setupEvents::FileChecker\n";
    } else if (UseOneChecker == "DataLoss") {
      checker = new DataLossChecker(NULL);
      errs() << "EventMgr::setupEvents::DataLossChecker\n";
    } else if (UseOneChecker == "Leak") {
      checker = new LeakChecker(NULL);
      errs() << "EventMgr::setupEvents::LeakChecker\n";
    } else
      assert(false && "UseOneChecker must be Assert, OpenClose, Lock, File, Leak or DataLoss.");
  }
  
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    if (f->hasName()) {
      if (!checker) {
        fprintf(stderr, "EventMgr::setupEvents checker is NULL, please use only one checker.\n");
        exit(1);
      } else if (checker->mayBeEvent(f->getNameStr())) { // Select events based on the checker.
        fprintf(stderr, "EventMgr::setupEvents event %s\n", f->getNameStr().c_str());
        eventFuncs.insert(f);
      }
    }
  }
  module = &M;
}

bool EventMgr::isEventCall(Instruction *instr) {
  if (!checker) {
    fprintf(stderr, "EventMgr::isEventCall checker is NULL, please use only one checker.\n");
    exit(1);
  }
  if (DS_IN(instr, eventCallSites)) {
    //errs() << "EventMgr::isEventCall yes: " << *instr << "\n";
    return true;
  }
  //errs() << "EventMgr::isEventCall no: " << *instr << "\n";
  return false;
}

void EventMgr::DFS(Function *f) {
  visited.insert(f);
  InstList css = CG->get_call_sites(f);
  fprintf(stderr, "EventMgr::DFS traverse function %s, callsite " SZ "\n",
    f->getNameStr().c_str(), css.size());
  for (size_t i = 0; i < css.size(); ++i) {
    if (isIgnoredEventCall(css[i], f))
      continue;
    Function *caller = css[i]->getParent()->getParent();
    //errs() << "EventMgr::DFS callsite caller " << caller->getNameStr() << ":" << *(css[i]) << "\n";
    if (visited.count(caller) == 0) {
      parent[caller] = f;
      DFS(caller);
    }
  }
}

bool EventMgr::mayCallEvent(Function *f) {
  return visited.count(f);
}

bool EventMgr::eventBetween(BranchInst *prevInstr, Instruction *postInstr) {
  Function *func = Util::getFunction(prevInstr);
  BasicBlock *postDomBB = NULL;
  if (postInstr)
    postDomBB = Util::getBasicBlock(postInstr);
  
  /* Flood fill from <bb> until reaching <post_dominator_bb> */
  bbVisited.clear();
  for (Function::iterator bi = func->begin(); bi != func->end(); ++bi)
    bbVisited[bi] = false;
  for (unsigned i = 0; i < prevInstr->getNumSuccessors(); i++)
    DFS(prevInstr->getSuccessor(i), postDomBB);
  /* If any visited BB has a sync operation, the branch cannot be omitted. */
  for (Function::iterator bi = func->begin(); bi != func->end(); ++bi) {
    if (!bbVisited[bi])
      continue;
    // cerr << "Visited BB: " << bi->getNameStr() << endl;
    for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
      if (is_call(ii) && !is_intrinsic_call(ii)) {
        vector<Function *> called_funcs = CG->get_called_functions(ii);
        for (size_t i = 0; i < called_funcs.size(); ++i) {
          if (mayCallEvent(called_funcs[i]))
            return true;
        }
      }
    }
  }
  return false;
}

void EventMgr::DFS(BasicBlock *x, BasicBlock *sink) {
  // cerr << "DFS: " << x->getNameStr() << endl;
  if (bbVisited[x])
    return;
  // Stop at the sink -- the post dominator of the branch
  if (x == sink)
    return;
  bbVisited[x] = true;
  for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
    BasicBlock *y = *it;
    DFS(y, sink);
  }
}

void EventMgr::traverse_call_graph(Module &M) {
  visited.clear();
  parent.clear();
  DenseSet<Function *>::iterator itr(eventFuncs.begin());
  for (; itr != eventFuncs.end(); ++itr) {
    collectStaticEventCalls(*itr);
    DFS(*itr);
  }

  // DBG.
  DenseSet<Function *>::iterator itrVisited(visited.begin());
  for (; itrVisited != visited.end(); ++itrVisited) {
    Function *f = *itrVisited;
    errs() << "EventMgr::traverse_call_graph may call event function " << f->getNameStr() << "\n";
  }
}

bool EventMgr::runOnModule(Module &M) {
  assert(CG);
  setupEvents(M);
  traverse_call_graph(M);  
  return false;
}

size_t EventMgr::numEventCallSites() {
  return eventCallSites.size();
}

void EventMgr::printEventCalls() {
  DenseSet<Instruction *>::iterator itr(eventCallSites.begin());
  for (Module::iterator f = module->begin(), fe = module->end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        Instruction *instr = i;
        if (eventCallSites.count(instr) > 0) {
  	      errs() << "EventMgr::printEventCalls static " << Util::printNearByFileLoc(instr)
           << " " << f->getNameStr() << ":" 
           << b->getNameStr() << ":" << *(instr) << "\n\n";
        }
      }
}

void EventMgr::collectStaticEventCalls(Function *event) {
  InstList css = CG->get_call_sites(event);
  for (size_t i = 0; i < css.size(); ++i) {
    if (isIgnoredEventCall(css[i], event))
      continue;
    eventCallSites.insert(css[i]);
  }
}

bool EventMgr::isIgnoredEventCall(Instruction *call, Function *event) {
  int argOffset = -1;
  std::string eventName = event->getNameStr();
  if (eventName == "fclose" ||
    eventName == "fprintf" ||
    eventName == "ferror" ||
    eventName == "ferror_unlocked")
    argOffset = 0;
  else if (eventName == "fputs" ||
    eventName == "fputs_unlocked")
    argOffset = 1;
  else if (
    eventName == "fgets" ||
    eventName == "fgets_unlocked")
    argOffset = 2;
  else if (eventName == "fread" ||
    eventName == "fread_unlocked" ||
    eventName == "fwrite" ||
    eventName == "fwrite_unlocked")
    argOffset = 3;

  if (argOffset >= 0) {
    assert(Util::isCall(call));
    argOffset++;  // The "1" offset is the called function.
    Value *oprd = call->getOperand(argOffset);  
    errs() << "EventMgr::isIgnoredEventCall oprd: " << *oprd << "\n";
    if  (isStdErrOrOut(call->getParent(), oprd)) {
      errs() << "EventMgr::isIgnoredEventCall IGNORED event: " << *call << "\n";
      return true;
    }
  }

  errs() << "EventMgr::isIgnoredEventCall VALID event: " << *call << "\n";
  return false;  
}

/* Statically and conservatively (soundly) check whether a value v is @stderr or @stdout.
  Iff the value v is a @stderr or @stdout from current basic block, returns true. */
bool EventMgr::isStdErrOrOut(BasicBlock *curBB, Value *v) {
  Instruction *instr = dyn_cast<Instruction>(v);
  if (instr) {
    BasicBlock *bb = instr->getParent();
    if (bb != curBB || instr->getNumOperands() == 0)
      return false;
    Value *oprd0 = instr->getOperand(0);
    if (Util::isLoad(instr)) {
       if (oprd0->getNameStr() == "stderr" || oprd0->getNameStr() == "stdout")
         return true;
    } else
      return isStdErrOrOut(curBB, oprd0); /* Recursively look at the first operand,
      since sometimes it could be a bit cast instruction and then a load instruction. */
  }
  return false;
}

