/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "common/instr/instrutil.h"
#include "recorder/instr/loggable.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CallSite.h"
#include "logaccess.h"

#ifdef _DEBUG //_RECORDER
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

using namespace std;
using namespace llvm;

namespace tern {

RawLog::RawLog() : _fd(-1), _num(0), _buf(NULL) {}

RawLog::RawLog(const char* filename) {
  struct stat st;
  long pagesz;

  _fd = open(filename, O_RDONLY);  assert(_fd >= 0);
  fstat(_fd, &st);
  assert(st.st_size % RECORD_SIZE == 0
         && "file size is not multiple of record size!");
  _num = st.st_size / RECORD_SIZE;
  pagesz = getpagesize();
  _mapsz = ((st.st_size+pagesz-1)&~(pagesz-1));
  _buf = (char*)mmap(0, _mapsz, PROT_READ, MAP_SHARED, _fd, 0);

  if(_buf == MAP_FAILED) {
    perror("mmap");
    abort();
  }
}

RawLog::~RawLog() {
  if(_buf)
    munmap(_buf, _mapsz);
  if(_fd >= 0)
    close(_fd);
}

RawLog::iterator RawLog::begin() {
  return iterator(this, 0);
}

RawLog::iterator RawLog::end() {
  return iterator(this, _num);
}

RawLog::reverse_iterator RawLog::rbegin() {
  return reverse_iterator(end());
}

RawLog::reverse_iterator RawLog::rend() {
  return reverse_iterator(begin());
}


void InstLog::append(const RawLog::iterator &ri) {
  ExecutedInstID id;
  id.inst = ri.getIndex();
  id.isLogged = 1;
  instLog.push_back(id);
}

void InstLog::append(Instruction *I) {
  ExecutedInstID id;
  unsigned insid = getIDManager()->getInstructionID(I);

  assert(insid != INVALID_INSID && "instruction has no valid id!");
  // TODO assert IDM->size() not too large once and for all
  id.inst = insid;
  id.isLogged = 0;
  instLog.push_back(id);
}

/// this call should be the call that changed the execution from curBB to
/// nxtBB
void InstLogBuilder::incFromCall() {
  Function *F;
  BasicBlock *nxtBB;
  const CallSite cs(cur_ii);

  F = cs.getCalledFunction();
  if(F && !funcBodyLogged(F)) {
    ++ cur_ii;
    return;
  }

  // sanity: nxtBB is the entry of the called function
  nxtBB = nxt_ii->getParent();
  if(F == NULL) // indirect call
    F = nxtBB->getParent();
  assert(nxtBB == &F->getEntryBlock()
         && "no log record from a function entry block!");
  assert(nxtBB->getParent() == F
         && "call target is different than logged!");

  // return address
  BasicBlock::iterator ret_ii = cur_ii;
  ++ ret_ii;
  callStack.push(ret_ii);

  // move cur_ii
  cur_ii = nxtBB->begin();
}

void InstLogBuilder::incFromReturn() {
  // TODO sanity: the previous instruction of ii is a call to this function
  assert(!callStack.empty() && "return and call mismatch!");
  cur_ii = callStack.top();
  callStack.pop();
}

void InstLogBuilder::incFromJmp() {
  BasicBlock *nxtBB = nxt_ii->getParent();

  // sanity: nxtBB is one of the successors of curBB
  bool is_succ = false;
  BasicBlock *BB = cur_ii->getParent();
  for(succ_iterator si=succ_begin(BB); si!=succ_end(BB); ++si) {
    if(*si == nxtBB) {
      is_succ = true;
      break;
    }
  }
if(!is_succ) {
  errs() << "distance between raw iterators = " << nxt_ri - cur_ri << "\n";
  errs() << *cur_ri  << "\n";
  errs() << *nxt_ri  << "\n";
  errs() << "cur I"  << *cur_ii << "\n"
         << "nxt I"  << *nxt_ii << "\n";
  errs() << "cur BB" << *BB
         << "nxt BB" << *nxtBB;
}
  assert(is_succ && "successor BB of a branch is not logged!");
  cur_ii = nxtBB->begin();
}


void InstLogBuilder::getInstBetween() {
  unsigned cur_insid = cur_ri->getInsid();
  cur_ii = getIDManager()->getInstruction(cur_insid);
  unsigned nxt_insid = nxt_ri->getInsid();
  nxt_ii = getIDManager()->getInstruction(nxt_insid);

  if(getInstLoggedMD(cur_ii) != LogBBMarker) {
    instLog->append(cur_ri);
    ++ cur_ii;
  }

  while(cur_ii != nxt_ii) {
    instLog->append(cur_ii);

    // TODO: can only do one of these control transfer inst
    switch(cur_ii->getOpcode()) {
    case Instruction::Call:
    case Instruction::Invoke:
      incFromCall();
      break;
    case Instruction::Ret:
      incFromReturn();
      break;
    case Instruction::Unwind:
    case Instruction::IndirectBr:
    case Instruction::Switch:
    case Instruction::Br:
      incFromJmp();
      break;
    case Instruction::Unreachable:
      assert(0 && "InstLogBuilder should never reach "\
             "an Unrechable instruction!");
    default:
      ++ cur_ii;
      break;
    }
  }
}

void InstLogBuilder::getInstPrefix() {
  // TODO
}

void InstLogBuilder::getInstSuffix() {
  // TODO
}

InstLog *InstLogBuilder::create(const char *file) {
  RawLog *rawLog = new RawLog(file);
  return create(rawLog);
}


InstLog *InstLogBuilder::create(RawLog *log) {

  instLog = new InstLog(log);

  cur_ri = log->begin(); // thread_begin();
  nxt_ri =  cur_ri + NumRelatedRecords(*cur_ri); // first logged inst of the prog

  getInstPrefix(); // thread_begin() to first logged inst of the program

  bool calledPthreadExit = true;
  RawLog::iterator end_ri = log->end();
  -- end_ri;  // thread_end();
  if(!end_ri->validInsid()) {
    calledPthreadExit = false;
    -- end_ri; // last logged inst before ret from thread func (or main())
  }

  while(nxt_ri != end_ri) {
    cur_ri = nxt_ri;
    nxt_ri = cur_ri + NumRelatedRecords(*cur_ri);
    getInstBetween();
  }

  if(calledPthreadExit)
    instLog->append(end_ri); // append thread_end
  else
    getInstSuffix(); // last logged inst to ret from thread func (or main())

  return instLog;
}

raw_ostream &InstLog::printExecutedInst(raw_ostream &o,
                       ExecutedInstID id, bool details) {
  unsigned insid;
  if(id.isLogged) {
    o << "L idx=" << id.inst;
    raw_iterator ri(rawLog, id.inst);
    o << " " << *ri;
    insid = ri->insid;
  } else {
    o << "U ins=" << id.inst;
    insid = id.inst;
  }

  if(details) {
    Instruction *I = getIDManager()->getInstruction(insid);
    if(I)
      o << *I;
  }
  return o;
}

static const char* recNames[] = {
  "ins",
  "load",
  "store",
  "call",
  "arg",
  "ret",
  "sync",
};

static raw_ostream& printInsidRec(raw_ostream& o, const InsidRec& rec) {
  assert(rec.type <= LastRecTy && "invalid log record type!");
  if(rec.insid == tern::INVALID_INSID_IN_REC)
    o << "ins=n/a";
  else
    o << "ins= " << rec.insid;
  return o << " " << recNames[rec.type];
}

raw_ostream& operator<<(raw_ostream& o, const InsidRec& rec) {
  switch(rec.type) {
  case InsidRecTy:
    printInsidRec(o, rec);
    break;
  case LoadRecTy:
    o << (const LoadRec&)rec;
    break;
  case StoreRecTy:
    o << (const StoreRec&)rec;
    break;
  case CallRecTy:
    o << (const CallRec&)rec;
    break;
  case ExtraArgsRecTy:
    o << (const ExtraArgsRec&)rec;
    break;
  case ReturnRecTy:
    o << (const ReturnRec&)rec;
    break;
  case SyncRecTy:
    o << (const SyncRec&)rec;
    break;
  }
  return o;
}

raw_ostream &operator<<(raw_ostream &o, const LoadRec &rec) {
  return printInsidRec(o, (const InsidRec&)rec)
    << " " << rec.addr << " = " << rec.data;
}
raw_ostream &operator<<(raw_ostream &o, const StoreRec &rec) {
  return printInsidRec(o, (const InsidRec&)rec)
    << " " << rec.addr << ", " << rec.data;
}
raw_ostream &operator<<(raw_ostream &o, const CallRec &rec) {
  return printInsidRec(o, (const InsidRec&)rec)
    << " func" << rec.funcid << "(" << rec.narg << " args)";
}
raw_ostream &operator<<(raw_ostream &o, const ExtraArgsRec &rec) {
  return printInsidRec(o, (const InsidRec&)rec) << "(...)";
}
raw_ostream &operator<<(raw_ostream &o, const ReturnRec &rec) {
  return printInsidRec(o, (const InsidRec&)rec)
    << " func" << rec.funcid << "() = " << rec.data;
}
raw_ostream &operator<<(raw_ostream &o, const SyncRec &rec) {
  printInsidRec(o, (const InsidRec&)rec)
    << " " << syncfunc::getName(rec.sync) << "()";
  if(NumRecordsForSync(rec.sync) == 2)
     o << (rec.after?"after":"before");
  return o << " turn = " << rec.turn;
}

} // namespace tern
