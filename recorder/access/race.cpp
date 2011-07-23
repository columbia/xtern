/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include "race.h"
#include "util.h"

//#define dprintf(args...) fprintf(stderr, args)
#define dprintf(args...)

using namespace llvm;
using namespace tern;

Access::Access(bool isWr, const InstLog::Trunk *c,
               const InstLog::DInst &ins, uint8_t d)
  : isWrite(isWr), ts(c), inst(ins), data(d) { }

Access::Access(const Access &a)
  : isWrite(a.isWrite), ts(a.ts), inst(a.inst), data(a.data) { }

/***/

AccessHistory::AccessHistory(void *address): addr(address) {}

AccessHistory::AccessHistory(void *address, Access *a) {
  addr = address;
  if(a->isWrite)
    writes.push_back(a);
  else
    reads.push_back(a);
}

AccessHistory::~AccessHistory()
{
  forall(std::list<Access*>, i, reads)
    delete *i;
  forall(std::list<Access*>, i, writes)
    delete *i;
}

void AccessHistory::removeReads(Access *access)
{
  removeAccesses(access, reads);
}

void AccessHistory::removeWrites(Access *access)
{
  removeAccesses(access, writes);
}

void AccessHistory::removeAccesses(Access *a, std::list<Access*>& accesses)
{
  std::list<Access*>::iterator cur, prv;
  for(cur=accesses.begin(); cur!=accesses.end();) {
    prv = cur++;
    if((*prv)->ts->happensBefore(*a->ts)) {
      Access *t = *prv;
      delete t;
      accesses.erase(prv);
    }
  }
}

void AccessHistory::printRace(Access *a1, Access *a2)
{
}

bool AccessHistory::appendAccess(Access *access)
{
  std::list<Access*> racy_accesses;

  if(access->isWrite) {
    // remove reads < access->ts
    removeReads(access);
    // remove writes < access->ts
    removeWrites(access);

    // detect write-read races
    if(!reads.empty())
      racy_accesses.insert(racy_accesses.begin(),
                           reads.begin(), reads.end());
    // detect write-write races
    if(!writes.empty())
      racy_accesses.insert(racy_accesses.begin(),
                           writes.begin(), writes.end());

    // add access to write list
    writes.push_back(access);

  } else {// access->isWrite == false
    // remove reads < access->ts
    removeReads(access);

    // detect read-write race
    std::list<Access*>::const_iterator i;
    for(i=writes.begin(); i!=writes.end(); ++i)
      if((*i)->ts->concurrent(*access->ts))
        racy_accesses.push_back(*i);

    // add access to read list
    reads.push_back(access);
  }

  int nrace = 0;
  forall(std::list<Access*>, i, racy_accesses) {
    Access *racy = *i;
    // assert(racy->tid != access->tid);

    /// prune benign races.  note that our goal here is to decide if the
    /// program may be nondeterministic when two instructions interleave
    /// differently.  we can prune away useless writes: (1) a write-write
    /// race where two writes are of the same value and (2) a read-write
    /// race where the write is of the same value as the value already
    /// read

    // \todo: should consider symbolic values when pruning

#if 0
    if(get_option(tern, prune_useless_writes)) {
      uint64_t v1, v2;
      if(access->isWrite) { // write-write race or read-write race
        v1 = cast<ConstantExpr>(racy->value)->getZExtValue(width*8);
        v2 = cast<ConstantExpr>(access->value)->getZExtValue(width*8);
      } else {  // write-read race
        v1 = cast<ConstantExpr>(racy->old_value)->getZExtValue(width*8);
        v2 = cast<ConstantExpr>(racy->value)->getZExtValue(width*8);
      }

      if(v1==v2) {
        klee_warning("pruned %s-%s race with value %ld\n",
                     racy->isWrite?"write":"read",
                     access->isWrite?"write":"read",
                     (long)v1);
        continue;
      }
    }
#endif

    // real race
    ++ nrace;
    printRace(racy, access);

#if 0
    // prune redundant race reports if necessary
    if(get_option(tern, suppress_redundant_races)) {
      std::list<Access*> *al = (racy->isWrite? &writes : &reads);
      std::list<Access*>::iterator j = find(al->begin(), al->end(), racy);
      assert(j != al->end());
      al->erase(j);
      //delete racy;
    }
#endif

  }

  return nrace > 0;
}

/***/

void RaceDetector::onMemoryAccess(InstLog::Trunk *tr,
                                  const InstLog::DInst &di)
{
  AccessHistory *ah;

  InstLog *log = tr->instLog;
  Instruction *I = log->getInst(di);

  int size = log->getSize(di);
  char *addr = (char*)log->getAddr(di);
  uint64_t data = log->getData(di);
  uint8_t *dataptr = (uint8_t*)&data;

  // for each byte addr
  for(int i=0; i<size; ++i, ++addr, ++dataptr) {
    AccessMap::iterator ai = accesses.find(addr);
    if(ai == accesses.end()) {
      ah = new AccessHistory(addr);
      accesses[addr] = ah;
    } else {
      ah = ai->second;
    }
    Access *access = new Access(I->getOpcode()==Instruction::Store,
                                tr, di, *dataptr);
    ah->appendAccess(access);
  }
}

RaceDetector::RaceDetector() {}

RaceDetector::~RaceDetector()
{
  forall(AccessMap, i, accesses) {
    delete i->second;
  }
}

void RaceDetector::install() {
  RaceDetector::the = new RaceDetector();
}

RaceDetector *RaceDetector::the = NULL;
