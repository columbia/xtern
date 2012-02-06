#include "event-func.h"
using namespace tern;

using namespace llvm;

using namespace std;

char tern::EventFunc::ID = 0;

namespace {
  static RegisterPass<tern::EventFunc> X(
    "event-func",
    "Get functions with event operations",
    false,
    true); // is analysis
}

void EventFunc::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  CallGraphFP::getAnalysisUsage(AU);
}

void EventFunc::setupEvents(std::vector<llvm::Function *> &eventList) {
  sync_funcs.clear();
  sync_funcs.insert(sync_funcs.begin(), eventList.begin(), eventList.end());
}

// TODO: search in vector may be slow
bool EventFunc::is_sync_function(Function *f) {
  for (size_t i = 0; i < sync_funcs.size(); ++i) {
    if (sync_funcs[i] == f)
      return true;
  }
  return false;
}

void EventFunc::DFS(Function *f) {
  visited.insert(f);
  InstList css = get_call_sites(f);
  for (size_t i = 0; i < css.size(); ++i) {
    Function *caller = css[i]->getParent()->getParent();
    if (visited.count(caller) == 0) {
      parent[caller] = f;
      DFS(caller);
    }
  }
}

void EventFunc::output(const Module &M) const {
  vector<string> func_names;
  FILE *fout;

  func_names.clear();
  for (Module::const_iterator fi = M.begin(); fi != M.end(); ++fi)
    func_names.push_back(fi->getNameStr());
  sort(func_names.begin(), func_names.end());
  fout = fopen("/tmp/all-func.txt", "w");
  for (size_t i = 0; i < func_names.size(); ++i) {
    fprintf(fout, "%s\n", func_names[i].c_str());
  }

  func_names.clear();
  for (DenseSet<Function *>::const_iterator it = visited.begin();
      it != visited.end(); ++it) {
    func_names.push_back((*it)->getNameStr());
  }
  sort(func_names.begin(), func_names.end());
  fout = fopen("/tmp/event-func.txt", "w");
  for (size_t i = 0; i < func_names.size(); ++i) {
    fprintf(fout, "%s\n", func_names[i].c_str());
  }
}

bool EventFunc::contains_sync_operation(Function *f) {
  return visited.count(f);
}

void EventFunc::traverse_call_graph(Module &M) {
  visited.clear();
  parent.clear();
  for (size_t i = 0; i < sync_funcs.size(); ++i) {
    DFS(sync_funcs[i]);
  }
}

void EventFunc::print_call_chain(Function *f) {
  /*if (!parent.count(f)) {
    std::cerr << endl;
    return;
  }
  std::cerr << f->getNameStr() << " => ";
  print_call_chain(parent[f]);*/
}

void EventFunc::print_calling_functions(Function *f) {
  /*if (!call_sites.count(f))
    return;
  cerr << f->getNameStr() << ": ";
  vector<Instruction *> &callers = call_sites[f];
  for (size_t i = 0; i < callers.size(); ++i) {
    cerr << callers[i]->getParent()->getParent()->getNameStr() << ' ';
  }
  cerr << endl;*/
}

void EventFunc::stats(const Module &M) const {

}

bool EventFunc::runOnModule(Module &M) {
  CallGraphFP::runOnModule(M);
  traverse_call_graph(M);
  return false;
}

void EventFunc::print(llvm::raw_ostream &O, const Module *M) const {
  output(*M);
  stats(*M);
}

