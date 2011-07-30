/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

#include "util.h"
#include "common/instr/instrutil.h"
#include "recorder/instr/loggable.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CallSite.h"
#include "logaccess.h"
#include "race.h"

//#define _DEBUG_LOGACCESS

#ifdef _DEBUG_LOGACCESS
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


char *LMemInst::getAddr() const {
  assert(rec->type == LoadRecTy || rec->type == StoreRecTy);
  MemRec *rec = (MemRec*)this->rec;
  return rec->getAddr();
}

unsigned LMemInst::getDataSize() const {
  assert(rec->type == LoadRecTy || rec->type == StoreRecTy);
  TargetData *TD = getTargetData();
  switch(inst->getOpcode()) {
  case Instruction::Load:
    return TD->getTypeStoreSize(inst->getType());
  case Instruction::Store:
    return TD->getTypeStoreSize(dyn_cast<StoreInst>(inst)
                                ->getOperand(0)->getType());
  default:
    assert(0);
  }
}

uint64_t LMemInst::getData() const {
  assert(rec->type == LoadRecTy || rec->type == StoreRecTy);
  MemRec *rec = (MemRec*)this->rec;
  return rec->getData();
}

uint8_t LMemInst::getDataByte(unsigned bytenr) const {
  assert(rec->type == LoadRecTy || rec->type == StoreRecTy);
  MemRec *rec = (MemRec*)this->rec;
  int64_t data = rec->getData();
  assert(bytenr < getDataSize());
  return ((int8_t*)&data)[bytenr];
}

InstLog::InstLog(RawLog *log) {
  rawLog = log;
  // rule of thumb: load/store are 1/5 of all instructions
  instLog.reserve(rawLog->numRecords()*5);
}

unsigned InstLog::append(const RawLog::iterator &ri) {
  // append to trunks
  if(ri->type == SyncRecTy) {
    SyncRec *rec = (SyncRec*)&*ri;
    if(!trunks.empty()) {
      InstTrunk &last = trunks.back();
      last.endTurn = rec->turn;
      last.endIndex = numDInsts();
    }
    if(rec->sync != syncfunc::tern_thread_end)
      trunks.push_back(InstTrunk(this, rec->turn, numDInsts()));
  }

  // append to instLog
  DInst di;
  unsigned idx;

  di.inst = ri.getIndex();
  di.isLogged = 1;
  idx = instLog.size();
  instLog.push_back(di);

#ifdef _DEBUG_LOGACCESS
  printDInst(errs(), idx, true) << "\n";
#endif

  return idx;
}

unsigned InstLog::append(Instruction *I) {
  DInst di;
  unsigned idx;
  unsigned insid = getIDManager()->getInstructionID(I);
if(insid == INVALID_INSID)
errs() << *I << "\n";
  assert(insid != INVALID_INSID && "instruction has no valid id!");
  // TODO assert getIDManager()->size() not too large once and for all
  if(I->getOpcode() == Instruction::Load
     || I->getOpcode() == Instruction::Store) {
errs() << *I << "\n";
  }
  assert(I->getOpcode() != Instruction::Load
         && I->getOpcode() != Instruction::Store);
  di.inst = insid;
  di.isLogged = 0;
  idx = instLog.size();
  instLog.push_back(di);

#ifdef _DEBUG_LOGACCESS
  printDInst(errs(), idx, true) << "\n";
#endif

  return idx;
}

void InstLog::matchCallReturn(unsigned callIdx, unsigned retIdx) {
  assert(callRetMap.find(callIdx) == callRetMap.end());
  assert(retCallMap.find(retIdx)  == retCallMap.end());
  callRetMap[callIdx] = retIdx;
  retCallMap[retIdx]  = callIdx;
}

raw_ostream &InstLog::printDInst(raw_ostream &o, unsigned idx, bool details) {
  unsigned insid;
  DInst di = instLog[idx];

  if(di.isLogged) {
    o << "L idx=" << di.inst;
    raw_iterator ri(rawLog, di.inst);
    o << " ";
    printRawRec(o, ri);
    insid = ri->insid;
  } else {
    o << "P ins=" << di.inst;
    insid = di.inst;
  }

  if(details) {
    Instruction *I = getIDManager()->getInstruction(insid);
    if(I)
      o << *I;
  }
  return o;
}

bool InstLog::isDInstLogged(unsigned idx) {
  DInst di = instLog[idx];
  return di.isLogged;
}

LInst InstLog::getLInst(unsigned idx) {
  DInst di = instLog[idx];
  assert(di.isLogged);
  raw_iterator ri(rawLog, di.inst);
  unsigned insid = ri->getInsid();

  if(insid == INVALID_INSID)
    return LInst(NULL, ri);

  Instruction *I = getIDManager()->getInstruction(insid);
  return LInst(I, ri);
}

Instruction *InstLog::getStaticInst(unsigned idx) {
  unsigned insid;
  DInst di = instLog[idx];
  assert(!di.isLogged);
  insid = di.inst;
  return getIDManager()->getInstruction(insid);
}

llvm::Function *InstLog::getCalledFunction(unsigned callIdx) {
  unsigned retIdx = getReturnFromCall(callIdx);
  Instruction *ret = getStaticInst(retIdx);
  return ret->getParent()->getParent();
}

unsigned InstLog::getReturnFromCall (unsigned callIdx) {
  IndexMap::iterator i = callRetMap.find(callIdx);
  assert(i != callRetMap.end());
  return i->second;
}

unsigned InstLog::getCallFromReturn (unsigned retIdx) {
  IndexMap::iterator i = retCallMap.find(retIdx);
  assert(i != retCallMap.end());
  return i->second;
}

BasicBlock *InstLog::getJumpTarget(unsigned idx) {

#ifdef _DEBUG
  // sanity
  Instruction  *I = getStaticInst(idx);
  switch(I->getOpcode()) {
  case Instruction::Unwind:
  case Instruction::IndirectBr:
  case Instruction::Switch:
  case Instruction::Br:
    break;
  default:
    assert(0);
  }
#endif

  DInst di = instLog[idx + 1];
  Instruction * nxtI;
  if(!di.isLogged)
    nxtI = getStaticInst(idx+1);
  else
    nxtI =getLInst(idx+1).inst;
  return nxtI->getParent();
}

BasicBlock *InstLog::getJumpSource(unsigned idx) {

#ifdef _DEBUG
  // sanity
  Instruction  *I = getStaticInst(idx);
  assert(I && I == I->getParent()->begin());
#endif

  DInst di = instLog[idx - 1];
  Instruction * prvI;
  // jump instructions are not logged
  assert(!isDInstLogged(idx-1));
  prvI = getStaticInst(idx-1);
  return prvI->getParent();
}

static void assertEntryInst(Instruction *I) {
  BasicBlock  *BB;
  Function    *F;
  BB = I->getParent();
  assert(I == BB->begin());
  F = BB->getParent();
  assert(BB == &F->getEntryBlock());
}

static void assertNextInst(Instruction *I, Instruction *nxtI) {
  BasicBlock::iterator i = I;
  assert(nxtI == ++i);
}

static void assertInvokeNormalDest(Instruction *I, Instruction *nxtI) {
  InvokeInst *invoke = dyn_cast<InvokeInst>(I);
  assert(nxtI == invoke->getNormalDest()->begin());
}

void InstLog::verify() {
  BasicBlock::iterator i;
  InsidRec    *rec;
  SyncRec     *syncRec;
  Instruction *I, *nxtI;
  BasicBlock  *BB;
  Function    *F;
  bool        is_succ;
  unsigned    last;

  // first DInst is syncfunc::tern_thread_begin
  assert(isDInstLogged(0));
  rec = getLInst(0).rec;
  assert(rec->type == SyncRecTy);
  syncRec = (SyncRec*)rec;
  assert(syncRec->sync == syncfunc::tern_thread_begin);

  // last DInst is syncfunc::tern_thread_end
  last = numDInsts()-1;
  assert(isDInstLogged(last));
  rec = getLInst(last).rec;
  assert(rec->type == SyncRecTy);
  syncRec = (SyncRec*)rec;
  assert(syncRec->sync == syncfunc::tern_thread_end);

  // second DInst is the entry to main() or a thread function
  if(isDInstLogged(1))
    I = getLInst(1).inst;
  else
    I = getStaticInst(1);
  assertEntryInst(I);

  // FIXME: this is bulgy ...
  for(unsigned i=1; i<numDInsts()-2; ++i) {
    if(isDInstLogged(i))
      I = getLInst(i).inst;
    else
      I = getStaticInst(i);

    if(isDInstLogged(i+1))
      nxtI = getLInst(i+1).inst;
    else
      nxtI = getStaticInst(i+1);

    assert(I && nxtI);

    switch(I->getOpcode()) {
    case Instruction::Load:
      assert(isDInstLogged(i));
      rec = getLInst(i).rec;
      assert(rec->type == LoadRecTy);
      break;
    case Instruction::Store:
      assert(isDInstLogged(i));
      rec = getLInst(i).rec;
      assert(rec->type == StoreRecTy);
      break;
    case Instruction::Ret:
      // TODO
      break;
    case Instruction::Unwind:
    case Instruction::IndirectBr:
    case Instruction::Switch:
    case Instruction::Br:
      is_succ = false;
      BB = I->getParent();
      for(succ_iterator si=succ_begin(BB); si!=succ_end(BB); ++si) {
        if(*si == nxtI->getParent()) {
          is_succ = true;
          break;
        }
      }
      if(!is_succ){
        errs() << *I << "\n";
        errs() << *nxtI << "\n";
      }
      assert(is_succ);
      break;
    case Instruction::Unreachable:
      assert(0 && "should never see unreachable in executed instructions!");
      break;
    case Instruction::Call:
    case Instruction::Invoke:
      if(!isDInstLogged(i)) {
        CallSite cs(I);
        F = cs.getCalledFunction();
        if(F) {
          if(funcBodyLogged(F)) {
            assert(F == nxtI->getParent()->getParent());
            assertEntryInst(nxtI);
            break;
          }
        } else { // indirect call, not logged
          if(I->getParent() != nxtI->getParent()) {
            assertEntryInst(nxtI);
            break;
          }
        }
      } // fall through
    default:
      if(!isDInstLogged(i)) {
        assertNextInst(I, nxtI);
        break;
      }
      rec = getLInst(i).rec;
      if(rec->type != SyncRecTy) {
        if(I->getOpcode() != Instruction::Invoke)
          assertNextInst(I, nxtI);
        else
          assertInvokeNormalDest(I, nxtI);
        break;
      }
      syncRec = (SyncRec*)rec;
      if(syncRec->after) {
        assertNextInst(I, nxtI);
        break;
      }
      if((syncRec->sync != syncfunc::pthread_cond_wait
          && syncRec->sync != syncfunc::pthread_barrier_wait)) {
        assertNextInst(I, nxtI);
        break;
      }
      assert(I == nxtI);
    }
  }
}

raw_ostream& operator<<(raw_ostream& o, const InstTrunk& tr) {
  return o << "[" << tr.beginTurn << "(" << tr.beginIndex << "), "
           << tr.endTurn << "(" << tr.endIndex << "))";
}


InstLog::func_map         InstLog::funcsEscape;
InstLog::func_map         InstLog::funcsCallLogged;
InstLog::reverse_func_map InstLog::funcsIDMap;

unsigned InstLog::getFuncID(Function *F) {
  func_map::iterator fi = funcsCallLogged.find(F);
  if(fi == funcsCallLogged.end())
    return (unsigned)-1;
  return fi->second;
}

Function *InstLog::getFunc(unsigned funcid) {
  reverse_func_map::iterator fi = funcsIDMap.find(funcid);
  if(fi == funcsIDMap.end())
    return NULL;
  return fi->second;
}

void InstLog::setFuncMap(const char *file, const Module& M) {
  ifstream f(file);
  assert(f && "can't open func map file for read!");

  unsigned funcid;
  unsigned escape;
  string funcname;
  for(;;) {
    f >> funcid;   if(!f) break;
    f >> escape;   if(!f) break;
    f >> funcname; if(!f) break;

    assert(funcid > Intrinsic::num_intrinsics
           && "invalid func id in .funcs file");
    assert((escape==0||escape==1)
           && "invalid escape value id in .funcs file");
    assert(!funcname.empty()
           && "invalid func name in .funcs file");

    Function *F = M.getFunction(funcname);
    if(!F) // unused function
      continue;
    assert(funcsCallLogged.find(F) == funcsCallLogged.end()
           && "redundant function names in .funcs file!");
    funcsCallLogged[F] = funcid;
    funcsIDMap[funcid] = F;
    if(escape)
      funcsEscape[F] = escape;
  }
}




/// this call should be the call that changed the execution from curBB to
/// nxtBB
int InstLogBuilder::nextInstFromCall() {
  Function *F;
  BasicBlock *nxtBB;
  const CallSite cs(cur_ii);

  F = cs.getCalledFunction();
  if(F && !funcBodyLogged(F)) {
    nextInstFromLoggedInst();
    return 0;
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
  InvokeInst *invoke;
  BasicBlock::iterator ret_ii = cur_ii;
  switch(cur_ii->getOpcode()) {
  case Instruction::Call:
    ++ ret_ii;
    break;
  case Instruction::Invoke:
    invoke = dyn_cast<InvokeInst>(ret_ii);
    ret_ii = invoke->getNormalDest()->begin();
    break;
  default:
    assert(0 && "not a call or invoke!");
  }

  callStack.push(pair<BasicBlock::iterator, unsigned>(ret_ii, cur_idx));

#ifdef _DEBUG_LOGACCESS
  errs() << "calling " << F->getName() << " return address "
         << getIDManager()->getInstructionID(ret_ii) << "\n";
#endif
  // move cur_ii
  cur_ii = nxtBB->begin();

  return 1;
}

int InstLogBuilder::nextInstFromReturn() {
  // TODO sanity: the previous instruction of ii is a call to this function
#ifdef _DEBUG_LOGACCESS
  errs() << "returning from " << cur_ii->getParent()->getParent()->getName()
         << " to return address "
         << getIDManager()->getInstructionID(callStack.top().first)
         << "\n";
#endif

  assert(!callStack.empty() && "return and call mismatch!");
  cur_ii   = callStack.top().first;
  call_idx = callStack.top().second;
  callStack.pop();

  // match call_idx and cur_idx;
  instLog->matchCallReturn(call_idx, cur_idx);

  return 1;
}

int InstLogBuilder::nextInstFromJmp() {
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
#ifdef _DEBUG_LOGACCESS
  if(!is_succ)
    dump();
#endif
  assert(is_succ && "successor BB of a branch is not logged!");
  cur_ii = nxtBB->begin();

  return 1;
}

void InstLogBuilder::nextInstFromLoggedInst() {
  if(cur_ii->getOpcode() != Instruction::Invoke) {
    ++ cur_ii;
    return;
  }
  InvokeInst *invoke = dyn_cast<InvokeInst>(cur_ii);
  cur_ii = invoke->getNormalDest()->begin();
}

void InstLogBuilder::appendInbetweenInsts(bool takeCurrent) {
  unsigned cur_insid = cur_ri->getInsid();
  cur_ii = getIDManager()->getInstruction(cur_insid);

  bool hasNxt = false;
  unsigned nxt_insid = nxt_ri->getInsid();
  if(nxt_insid != INVALID_INSID) {
    hasNxt = true;
    nxt_ii = getIDManager()->getInstruction(nxt_insid);
  }

  if(takeCurrent) {
    if(instLogged(cur_ii) != LogBBMarker) { // append current instruction
      assert(cur_ri->numRecForInst() == 1
             && "multiple records for one inst must be specially handled!");
      instLog->append(cur_ri);
      nextInstFromLoggedInst();
    }
  } else
      nextInstFromLoggedInst(); // skip current instruction

  //
  // can control-transfer more than once.  consider the example below:
  //
  // bb:
  //    %1 = call foo();
  //    br i1 %1, label %bb1, label bb2;
  //
  // suppose current instruction is the ret instruction from foo(), then
  // we'll have to process br as well since there's no instruction logged
  // between the two instructions shown above
  //
  int transfers = 0;
  for(;;) {
    if(hasNxt &&  cur_ii == nxt_ii)
        break;

    unsigned opcode = cur_ii->getOpcode();

    // previous instruction is a call that does not return ... stop
    if(opcode == Instruction::Unreachable)
      break;

    cur_idx = instLog->append(cur_ii);

    // return with empty call stack ==> must be a return from main or
    // thread func; stop
    if(opcode == Instruction::Ret && callStack.empty())
      break;

    switch(opcode) {
    case Instruction::Call:
    case Instruction::Invoke:
      transfers += nextInstFromCall();
      break;
    case Instruction::Ret:
      transfers += nextInstFromReturn();
      break;
    case Instruction::Unwind:
    case Instruction::IndirectBr:
    case Instruction::Switch:
    case Instruction::Br:
      transfers += nextInstFromJmp();
      break;
    default:
      ++ cur_ii;
      break;
    }
  }
}

void InstLogBuilder::appendInsts() {
  // sanity
  unsigned insid = cur_ri->getInsid();
  BasicBlock::iterator ii = getIDManager()->getInstruction(insid);
  for(RawLog::iterator ri=cur_ri; ri!=nxt_ri; ++ri)
    assert(ri->insid == insid
           && "records are supposedly for the same instruction, "\
           "but have different instruction IDs!");

  if(cur_ri->type == CallRecTy) { // just append one raw record to inst log
    instLog->append(cur_ri);
  } else { // a sync op; append all raw records from this op
    for(int i=0; i<cur_ri->numRecForInst(); ++i)
      instLog->append(cur_ri+i);
  }

  // get the instructions from the last raw record of the current
  // instruction to the next raw record; need this to handle cases such as
  //
  //  call foo (...)   <=== two raw records
  //  %2 = %1 * 2      <=== no raw records
  //  store %2, ...    <=== one raw record
  //
  cur_ri += cur_ri->numRecForInst()-1;
  appendInbetweenInsts(false);

}

void InstLogBuilder::appendInstsPrefix() {

  instLog->append(cur_ri); // syncfunc::tern_thread_begin

  unsigned nxt_insid = nxt_ri->getInsid();
  nxt_ii = getIDManager()->getInstruction(nxt_insid);

  BasicBlock *nxtBB = nxt_ii->getParent();
  for(cur_ii=nxtBB->begin(); cur_ii!=nxt_ii; ++cur_ii)
    instLog->append(cur_ii);
}

InstLog *InstLogBuilder::create(const char *file) {
  RawLog *rawLog = new RawLog(file);
  return create(rawLog);
}


// FIXME: relying on correctly finding the end of a raw log may be
// problematic if the logged thread didn't exit properly (so that our
// tern_thread_end() did not get called
InstLog *InstLogBuilder::create(RawLog *log) {

  instLog = new InstLog(log);

  cur_ri = log->begin(); // syncfunc::tern_thread_begin
  nxt_ri =  cur_ri + 1;  // first logged inst of the prog

  appendInstsPrefix();   // syncfunc::tern_thread_begin to first logged
                         // inst of the program

  RawLog::iterator end_ri = log->end();
  -- end_ri;  // syncfunc::tern_thread_end

  int nrec;
  while(nxt_ri != end_ri) {

    // also exit if nxt_ri points to syncfunc::tern_thread_end.
    if(nxt_ri->type == SyncRecTy) {
      const SyncRec& syncRec = (const SyncRec&)*nxt_ri;
      if(syncRec.sync == syncfunc::tern_thread_end)
        break;
    }

    cur_ri = nxt_ri;
    nrec = cur_ri->numRecForInst();
    nxt_ri = cur_ri + nrec;

    if(nrec > 1) // one instruction but multiple log recorsd
      appendInsts();
    else
      appendInbetweenInsts();
  }

  instLog->append(nxt_ri); // append syncfunc::tern_thread_end

#ifdef _DEBUG
  instLog->verify();
#endif

  return instLog;
}

void InstLogBuilder::dump() {
  BasicBlock *BB = cur_ii->getParent();
  BasicBlock *nxtBB = nxt_ii->getParent();
  errs() << "distance between raw iterators = " << nxt_ri - cur_ri << "\n";
  errs() << *cur_ri  << "\n";
  errs() << *nxt_ri  << "\n";
  errs() << "cur I"  << *cur_ii << "\n"
         << "nxt I"  << *nxt_ii << "\n";
  errs() << "cur BB" << *BB
         << "nxt BB" << *nxtBB;
}



void ProgInstLog::create(int nthread) {
  char logFile[64];
  InstLogBuilder builder;
  threadLogs.reserve(nthread);
  for(int i=0; i<nthread; ++i) {
    getLogFilename(logFile, sizeof(logFile), i);
    threadLogs[i] = builder.create(logFile);
  }

  for(int i=0; i<nthread; ++i) {
    for(InstLog::trunk_iterator ti=threadLogs[i]->trunk_begin();
        ti != threadLogs[i]->trunk_end(); ++ti) {
      unsigned turn = ti->beginTurn;
      trunks[turn] = &*ti;
    }
  }

  RaceDetector::install();

  // TODO: sanity check.  e.g., turn numbers must be continuous; no
  // redundant turns etc

  // race detection; clock is represented as just Trunk
  forall(TrunkMap, ti, trunks) {
    InstTrunk *tr = ti->second;
    for(unsigned idx=tr->beginIndex; idx<tr->endIndex; ++idx) {
      InstLog *log = tr->instLog;
      if(!log->isDInstLogged(idx))
        continue;
      LInst LI = log->getLInst(idx);
      if(LI.getRecType() != LoadRecTy
         && LI.getRecType() != StoreRecTy)
        continue;

      const LMemInst& MI = LInstCast<LMemInst>(LI);
      unsigned size = MI.getDataSize();
      char *addr = MI.getAddr();
      bool isWrite = (LI.getRecType() == StoreRecTy);

      for(unsigned i=0; i<size; ++i, ++addr) {
        RaceDetector::the->onMemoryAccess(isWrite, addr,
                                          MI.getDataByte(i), tr, idx);
      }
    }
  }

  RaceDetector::the->sortRacyAccesses(racyEdges);
}

static const char* recNames[] = {
  "marker",
  "load",
  "store",
  "call",
  "arg",
  "ret",
  "sync",
};

static raw_ostream& printInsidRec(raw_ostream& o, const InsidRec& rec) {
  assert(rec.type <= LastRecTy && "invalid log record type!");
  if(rec.validInsid())
    o << "ins=" << rec.insid;
  else
    o << "ins=n/a";
  return o << " " << recNames[rec.type];
}

static raw_ostream &printExtraArgsRec(raw_ostream &o, const ExtraArgsRec &rec) {
  for(short i=0; i<rec.numArgsInRec(); ++i) {
    o << ", " << rec.args[i];
  }
  return o;
}

static raw_ostream &printReturnRec(raw_ostream &o, const ReturnRec &rec) {
  return o << " = " << rec.data;
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
    << " " << (void*)rec.addr << " = " << rec.data;
}

raw_ostream &operator<<(raw_ostream &o, const StoreRec &rec) {
  return printInsidRec(o, (const InsidRec&)rec)
    << " " << (void*)rec.addr << ", " << rec.data;
}

raw_ostream &operator<<(raw_ostream &o, const CallRec &rec) {
  Function *F = InstLog::getFunc(rec.funcid);
  printInsidRec(o, (const InsidRec&)rec);
  if(F)
    o << " " << F->getName() << "(";
  else
    o << " func" << rec.funcid << "(";

  short i, narg = rec.numArgsInRec();
  for(i=0; i<narg; ++i) {
    o << rec.args[i] << ", ";
  }
  o << (rec.narg-narg) << " more args)";

  if(rec.flags & CallIndirect)
    o << " indirect";
  if(rec.flags & CallNoReturn)
    o << " noreturn";
  if(rec.flags & CalleeEscape)
    o << " escape";
  return o;
}

raw_ostream &operator<<(raw_ostream &o, const ExtraArgsRec &rec) {
  printInsidRec(o, (const InsidRec&)rec);
  printExtraArgsRec(o, (const ExtraArgsRec&)rec);
  return o;
}

raw_ostream &operator<<(raw_ostream &o, const ReturnRec &rec) {
  Function *F = InstLog::getFunc(rec.funcid);
  printInsidRec(o, (const InsidRec&)rec);
  if(F)
    o << " " << F->getName() << "()";
  else
    o << " func" << rec.funcid << "() ";
  return printReturnRec(o, rec);
}

raw_ostream &operator<<(raw_ostream &o, const SyncRec &rec) {
  printInsidRec(o, (const InsidRec&)rec)
    << " " << syncfunc::getName(rec.sync) << "()";
  if(NumRecordsForSync(rec.sync) == 2)
     o << (rec.after?"after":"before");
  return o << " turn = " << rec.turn;
}

raw_ostream &InstLog::printRawRec(raw_ostream &o,
                                  const InstLog::raw_iterator &ri) {
  if(ri->type != CallRecTy)
    return o << *ri;

  // CallRec
  const CallRec &rec = (const CallRec &)*ri;
  Function *F = InstLog::getFunc(rec.funcid);
  printInsidRec(o, (const InsidRec&)rec);
  if(F)
    o << " " << F->getName() << "(";
  else
    o << " func" << rec.funcid << "(";

  // Inline args
  short i, rec_narg = rec.numArgsInRec();
  for(i=0; i<rec_narg; ++i) {
    o << rec.args[i];
    if(i < rec.narg-1)
      o << ", ";
  }

  // ExtraArgsRec
  RawLog::iterator nxt_ri = ri;
  for(i=0; i<NumExtraArgsRecords(rec.narg); ++i) {
    ++ nxt_ri;
    printExtraArgsRec(o, (const ExtraArgsRec &)*nxt_ri);
  }

  o << ")";

  // ReturnRec
  if(!(rec.flags & CallNoReturn)) {
    ++ nxt_ri;
    printReturnRec(o, (const ReturnRec&)*nxt_ri);
  }

   // function attributes
  if(rec.flags & CallIndirect)
    o << " indirect";
  if(rec.flags & CallNoReturn)
    o << " noreturn";
  if(rec.flags & CalleeEscape)
    o << " escape";
  return o;
}

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const RacyEdgeHalf &reh) {
  return o << *reh.trunk << " " << reh.idx;
}

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const RacyEdge &re) {
  return o << re.up << ", " << re.down;
}


} // namespace tern
