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

IDManager *Log::IDM = NULL;

Log::iterator Log::begin() {
#if 0
  raw_iterator rbegin = raw_begin();
  if(rbegin != raw_end()) {
    unsigned insid = rbegin->insid;
    Instruction *ins = NULL; // find instruction based on insid
    BasicBlock::iterator ii = ins;
    return iterator(this, rbegin, ii);
  }
  return iterator(this, rbegin, BasicBlock::iterator());
#endif
}

Log::iterator Log::end() {
#if 0
  raw_iterator rend = raw_end();

  if(rend != raw_begin()) {
    raw_iterator rlast = rend - 1;
    unsigned insid = rlast->insid;
    Instruction *ins = NULL; // find instruction based on insid
    BasicBlock *BB = ins->getParent();
    return iterator(rend, BB->end());
  }
  return iterator(rend, 0); // FIXME
#endif
}

Log::reverse_iterator Log::rbegin() {
  return reverse_iterator(end());
}

Log::reverse_iterator Log::rend() {
  return reverse_iterator(begin());
}

/// this call should be the call that changed the execution from curBB to
/// nxtBB
void Log::incFromCall(const CallSite &cs) {
  Function *F;
  BasicBlock *nxtBB;

  // sanity: nxtBB is then entry of the called function
  F = cs.getCalledFunction();
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

void Log::incFromReturn(ReturnInst *I) {

  // TODO sanity: the previous instruction of ii is a call to this function
  assert(!callStack.empty() && "return and call mismatch!");
  cur_ii = callStack.top();
  callStack.pop();
}

void Log::incFromBr(BranchInst *I) {
  BasicBlock *nxtBB = nxt_ii->getParent();

  // sanity: nxtBB is one of the successors of curBB
  bool is_succ = false;
  BasicBlock *BB = I->getParent();
  for(succ_iterator si=succ_begin(BB); si!=succ_end(BB); ++si) {
    if(*si == nxtBB) {
      is_succ = true;
      break;
    }
  }
  assert(is_succ && "successor BB of a branch is not logged!");
  cur_ii = nxtBB->begin();
}

void Log::getInstBetween() {
  unsigned cur_insid = cur_ri->getInsid();
  cur_ii = IDM->getInstruction(cur_insid);
  unsigned nxt_insid = nxt_ri->getInsid();
  nxt_ii = IDM->getInstruction(nxt_insid);

  // if cur_ri is not a marker
  append(cur_ri);
  ++ cur_ii;

  for(;;) {
    switch(cur_ii->getOpcode()) {
    case Instruction::Call:
    case Instruction::Invoke:
      incFromCall(CallSite(cur_ii));
      break;
    case Instruction::Ret:
      incFromReturn(cast<ReturnInst>(cur_ii));
      break;
    case Instruction::Br:
      incFromBr(cast<BranchInst>(cur_ii));
      break;
    case Instruction::IndirectBr:
    case Instruction::Switch:
    case Instruction::Unreachable:
    case Instruction::Unwind:
      assert_not_supported();
    default:
      ++ cur_ii;
      break;
    }

    if(cur_ii == nxt_ii)
      break;

    append(cur_ii);
  }
}

void Log::createInstTrace() {
  cur_ri = raw_begin(); // thread_begin();
  nxt_ri =  cur_ri + NumRelatedRecords(*cur_ri); // first logged inst of the prog

  getInstPrefix(); // thread_begin() to first logged inst of the program

  bool calledPthreadExit = true;
  raw_iterator end_ri = raw_end();
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
    append(end_ri); // append thread_end
  else
    getInstSuffix(); // last logged inst to ret from thread func (or main())
}

#if 0
void Log::createInstTrace() {

  raw_iterator ri = raw_begin(), re = raw_end();
  while(ri->insid == INVALID_INSID_IN_REC && ri!=re)
    ++ ri;
  if(ri == re)
    return;

  Instruction *I = IDM->getInstruction(ri->insid);
  BasicBlock::iterator ii = I->getParent()->begin();
  if(I != ii)
    --ri;

  // ii, ri are our main iterators.  Invariants maintained:
  // ri <= ii <= ri + NumRelatedRecords(*ri) in the instruction strace
  //
  bool done = false;
  while(!done) {

    ExecutedInstID id;
    if(getLoggable(ii)) {
      id.isLogged = 1;
      id.inst = ri.getIndex();
    } else {
      id.isLogged = 0;
      id.inst = IDM->getInstructionID(ii);
    }

    instTrace.push_back(id);

    // move iterator
    switch(ii->getOpcode()) {
    case Instruction::Call:
    case Instruction::Invoke:
      incFromCall(CallSite(ii));
      break;
    case Instruction::Ret:
      incFromReturn(cast<ReturnInst>(ii));
      break;
    case Instruction::Br:
      incFromBr(cast<BranchInst>(ii));
      break;
    case Instruction::IndirectBr:
    case Instruction::Switch:
    case Instruction::Unreachable:
    case Instruction::Unwind:
      assert_not_supported();
    default:
      break;
    }
  }
}

bool Log::incFromCall(const CallSite &cs) {
  Function *F = cs.getCalledFunction();

  // check for tern functions
  if(F->getName().startswith("tern")) {
    if(F->getName() == "tern_pthread_exit") {
      assert(ri->type == SyncRecTy
             && ((const SyncRec&)*ri).sync == syncfunc::tern_thread_end);
      append(ri);
      return true;
    }

    for(int i=0; i<NumRecordsForSync(((const SyncRec&)*ri).sync); ++i)
      ;

    ++ ii;
    return false;
  }

  raw_iterator next_ri = ri + NumRelatedRecords(*ri);
  unsigned next_insid = next_ri->insid;

  assert(next_insid!=INVALID_INSID_IN_REC);

  bool logged = false;
  if(F && funcCallLogged(F)) {
    logged = true;
  } else if(!F) { // indirect call
    // peek into the next raw record and see if this indirect call is logged
    logged = (IDM->getInstructionID(ii) == next_insid);
  }

  if(logged) {
    ri = next_ri;
    ++ ii;
    return false;
  }

  // call is not logged; we'll step inside the function
  Instruction *nextI = IDM->getInstruction(next_insid);
  BasicBlock *nextBB = nextI->getParent();

  // sanity checking
  assert(nextBB == &nextBB->getParent()->getEntryBlock()
         && "no log record from a function entry block!");
  assert(!F || nextBB->getParent() == F
         && "call target is different than logged!");

  // return address
  BasicBlock::iterator ret_ii = ii;
  ++ ret_ii;
  callStack.push(ret_ii);

  // set ii and ri
  ii = nextBB->begin();
  if(getLoggable(ii))
    ri = next_ri;
}

void Log::incFromReturn(ReturnInst *I) {
  if(callStack.empty()) {
    // this is the return of a thread func or main(), set this iterator to
    // end().  i.e., set ii to end() of the current basic block, and ri to
    // the thread_end record in the raw log
    ++ ii;
    ri += NumRelatedRecords(*ri);
    assert(ii == ii->getParent()->end() && "iterator past return from "\
           "main or a thread function is not end() of its parent!");
    assert(ri->type == SyncRecTy
           && ((SyncRec&)*ri).sync == syncfunc::tern_thread_end
           && "raw record past return from main or a thread function"\
           " is not thread_end!");
    return;
  }

  ii = callStack.top();
  callStack.pop();
  if(getLoggable(ii))
    ri += NumRelatedRecords(*ri);
}

void Log::incFromBr(BranchInst *I) {
  raw_iterator next_ri = ri + NumRelatedRecords(*ri);
  unsigned next_insid = next_ri->insid;
  Instruction *nextI = IDM->getInstruction(next_insid);
  BasicBlock *nextBB = nextI->getParent();

  // sanity
  bool is_succ = false;
  BasicBlock *BB = I->getParent();
  for(succ_iterator si=succ_begin(BB); si!=succ_end(BB); ++si) {
    if(*si == nextBB) {
      is_succ = true;
      break;
    }
  }
  assert(is_succ && "cannot decide branch target based on raw log!!");

  ii = nextBB->begin();
  if(getLoggable(ii))
    ri = next_ri;
}

void Log::iterator::incFromCall(CallInst *I) {
  CallSite cs(I);
  Function *F = cs.getCalledFunction();

  raw_iterator next_ri = ri + NumRelatedRecords(*ri);
  unsigned next_insid = next_ri->insid;

  assert(next_insid!=INVALID_INSID_IN_REC);

  bool logged = false;
  if(F && funcCallLogged(F)) {
    logged = true;
  } else if(!F) { // indirect call
    // peek into the next raw record and see if this indirect call is logged
    logged = (IDM->getInstructionID(I) == next_insid);
  }

  if(logged) {
    ri = next_ri;
    ++ ii;
    return;
  }

  // call is not logged; we'll step inside the function
  Instruction *nextI = log->IDM->getInstruction(next_insid);
  BasicBlock *nextBB = nextI->getParent();

  // sanity checking
  assert(nextBB == &nextBB->getParent()->getEntryBlock()
         && "no log record from a function entry block!");
  assert(!F || nextBB->getParent() == F
         && "call target is different than logged!");

  // return address
  BasicBlock::iterator ret_ii = ii;
  ++ ret_ii;
  callStack.push(ret_ii);

  // set ii and ri
  ii = nextBB->begin();
  if(getLoggable(ii))
    ri = next_ri;
}

void Log::iterator::incFromReturn(ReturnInst *I) {
  if(callStack.empty()) {
    // this is the return of a thread func or main(), set this iterator to
    // end().  i.e., set ii to end() of the current basic block, and ri to
    // the thread_end record in the raw log
    ++ ii;
    ri += NumRelatedRecords(*ri);
    assert(ii == ii->getParent()->end() && "iterator past return from "\
           "main or a thread function is not end() of its parent!");
    assert(ri->type == SyncRecTy
           && ((SyncRec&)*ri).sync == syncfunc::tern_thread_end
           && "raw record past return from main or a thread function"\
           " is not thread_end!");
    return;
  }

  ii = callStack.top();
  callStack.pop();
  if(getLoggable(ii))
    ri += NumRelatedRecords(*ri);
}

void Log::iterator::incFromBr(BranchInst *I) {
  raw_iterator next_ri = ri + NumRelatedRecords(*ri);
  unsigned next_insid = next_ri->insid;
  Instruction *nextI = IDM->getInstruction(next_insid);
  BasicBlock *nextBB = nextI->getParent();

  // sanity
  bool is_succ = false;
  BasicBlock *BB = I->getParent();
  for(succ_iterator si=succ_begin(BB); si!=succ_end(BB); ++si) {
    if(*si == nextBB) {
      is_succ = true;
      break;
    }
  }
  assert(is_succ && "cannot decide branch target based on raw log!!");

  ii = nextBB->begin();
  if(getLoggable(ii))
    ri = next_ri;
}
#endif

Log::
iterator::self_type &Log::iterator::operator++() { // preincrement

  switch(ii->getOpcode()) {
  case Instruction::Call:
    incFromCall(cast<CallInst>(ii));
    break;
  case Instruction::Ret:
    incFromReturn(cast<ReturnInst>(ii));
    break;
  case Instruction::Br:
    incFromBr(cast<BranchInst>(ii));
    break;
  case Instruction::Invoke:
  case Instruction::IndirectBr:
  case Instruction::Switch:
  case Instruction::Unreachable:
  case Instruction::Unwind:
    assert_not_supported();
  default:
    break;
  }

  return *this;

#if 0
  BB = ii->getParent();
  nxt_ri = ri + NumRelatedRecords(*ri);

  if(ii == ret) {
    // check top of call stack

    // if not empty, pop stack

    // if empty, then this is a return from thread func or main

  } else if(ii == branch) {
  }

  if(ii == BB->end()) {
    if(next_ri == raw_end()) {
      // xxx
    }
    unsigned insid = next_ri->insid;
    if(insid == INVALID_INSID_IN_REC) {
      // xxx
    }
    Instruction *nxtIns = log->IDM->getInstruction(insid);
    nxtBB = nxtIns->getParent();

    Instruction *ret = callStack.top();
    if(ret == nxtIns) {
    }
  }

  if(ii != BB->end())
    return *this;
  ++ ri; // if call, find first
  ii = NULL; // get instruction from ri->insid
  return *this;
#endif

  /*

    invariant: ri <= ii < ri+ (num records of ri->type)
                              normally 1, for calls 2 + extra arg record

    ii ++;

    nxt_ri = ri + num records(ri->type);

    if(bb end)  {
      // find next basic block
      get the BB that nxt_ri belongs to
      if (BB = top of call stack) {
         assert that previous ii was actually a return instruction
         pop call stack
         ii = return ii;
         if(ii loggable)
           go to loggable processing
      }
    }   else if(call) {
      // these are only calls that are not logged, since previous branch
      // covered the logged case
      get the BB that ri belongs to
      push bb on call stack
      ii = BB->begin()
      if(ii) is loggable
         go to loggable processing
    }

    if(ii loggable) {
      assert ii and nxt_ri are same instruction
      remember ri
      ri = nxt_ri;
    }

   */
}

/*


  begin

  first case:  thread_begin  ... ii=BB.begin() ... ri
  second case: thread_begin  ... ri == BB.begin()  (first real instruction is loggable)

  ri = log.begin(); // this should be thread begin
  nxt_ri += numrec(ri); // this should be a real one

  // if nxt_ri is not the beginning of

  nxt_ri cannot be log.raw_end(), because for each log, at least two
  records (thread begin and thread end are there)

  find BB for ri.

  ii = BB->begin();

  if(ii is loggable)

    move ri


  end
    first case:  thread_end ...  nxt_ri == log end  (app calls pthread_exit)
                 ii = unrechable (instruction after pthread_exit)

    second case: ii == BB.end(), nxt_ri == thread_end (app does not call p.._exit)

    probably make them the same

 */



Log::iterator::self_type &Log::iterator::operator--() { // predecrement
  BasicBlock *BB = ii->getParent();
  if(ii != BB->begin()) {
    -- ii;

    // if loggable
    //   -- ri
    //   if return, move to call

    return *this;
  }

  -- ri; // if return, find call

  ii = NULL; // get instruction from ri->insid

  return *this;
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

raw_ostream& operator<<(raw_ostream& o, const InsidRec& rec) {
  assert(rec.type <= LastRecTy && "invalid log record type!");
  if(rec.insid == tern::INVALID_INSID_IN_REC)
    o << "ins=n/a";
  else
    o << "ins= " << rec.insid;
  return o << " " << recNames[rec.type];
}

raw_ostream &operator<<(raw_ostream &o, const LoadRec &rec) {
  return o << (const InsidRec&)rec << " " << rec.addr << " = " << rec.data;
}
raw_ostream &operator<<(raw_ostream &o, const StoreRec &rec) {
  return o << (const InsidRec&)rec << " " << rec.addr << ", " << rec.data;
}
raw_ostream &operator<<(raw_ostream &o, const CallRec &rec) {
  return o << (const InsidRec&)rec << " func" << rec.funcid
           << "(" << rec.narg << " args)";
}
raw_ostream &operator<<(raw_ostream &o, const ExtraArgsRec &rec) {
  return o << (const InsidRec&)rec << "(...)";
}
raw_ostream &operator<<(raw_ostream &o, const ReturnRec &rec) {
  return o << (const InsidRec&)rec << " func" << rec.funcid
           << "() = " << rec.data;
}
raw_ostream &operator<<(raw_ostream &o, const SyncRec &rec) {
  o << (const InsidRec&)rec << " " << syncfunc::getName(rec.sync) << "()";
  if(NumRecordsForSync(rec.sync) == 2)
     o << (rec.after?"after":"before");
  return o << " turn = " << rec.turn;
}

raw_ostream & PrintRecord(raw_ostream &o, const InsidRec &rec) {
  switch(rec.type) {
  case InsidRecTy:
    o << rec;
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
}

} // namespace tern
