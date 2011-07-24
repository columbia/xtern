/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include "race.h"
#include "util.h"

#define _DEBUG_RACEDETECTOR

#ifdef _DEBUG_RACEDETECTOR
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

using namespace llvm;

namespace tern {

Access::Access(bool isWr, uint8_t d, const InstTrunk *c, unsigned index)
  : isWrite(isWr), data(d), ts(c), idx(index) { }

Access::Access(const Access &a)
  : isWrite(a.isWrite), data(a.data), ts(a.ts), idx(a.idx) { }

raw_ostream &operator<<(raw_ostream &o, const Access &a) {
  return o << (a.isWrite?"write":"read") << " "
           << "data=" << (int)a.data << " "
           << "ts=[" << a.ts->beginTurn << "," << a.ts->endTurn << ")";
}

/***/

AccessHistory::AccessHistory(void *address): addr(address) {}

AccessHistory::AccessHistory(void *address, Access *a) {
  addr = address;
  appendAccessHelper(a);
}

AccessHistory::~AccessHistory() {
  forall(AccessMap, i, reads) {
    forall(AccessList, j, *(i->second)) {
      if(!(*j)->racy)
        delete *j;
    }
  }
  forall(AccessMap, i, writes) {
    forall(AccessList, j, *(i->second)) {
      if(!(*j)->racy)
        delete *j;
    }
  }
  forall(set<Access*>,  i, racyAccesses)
    delete *i;
}

void AccessHistory::removeReads(Access *access) {
  removeAccesses(access, reads);
}

void AccessHistory::removeWrites(Access *access) {
  removeAccesses(access, writes);
}

void AccessHistory::removeAccesses(Access *a, AccessMap &accesses) {

  list<const InstTrunk*> toErase;

  forall(AccessMap, i, accesses) {
    if(i->first->happensBefore(*a->ts))
      toErase.push_back(i->first);
  }

  forall(list<const InstTrunk*>, i, toErase) {
    AccessMap::iterator ai = accesses.find(*i);
    AccessList *al = ai->second;
    accesses.erase(ai);

#ifdef _DEBUG_RACEDETECTOR
    //errs() << "removing accesses for " << **i << "\n";
#endif

    forall(AccessList, j, *al) {
      if((*j)->racy) // otherwise, this access is owned by @racyAccesses
        delete *j;
    }
    delete al;
  }
}

void AccessHistory::dumpRace(const Access *a1, const Access *a2) const {
  errs() << "RACE on " << (void*) addr << ": "
         << (a1->isWrite?"write":"read") << "-"
         << (a2->isWrite?"write":"read") << " "
         << *a1 << " " << *a2 << "\n";
}

void AccessHistory::appendAccessHelper(Access *access) {

  AccessMap *am;

  if(access->isWrite)
    am = &writes;
  else
    am = &reads;

  AccessList *al;
  AccessMap::iterator ai = am->find(access->ts);
  if(ai != am->end())
    al = ai->second;
  else {
    al = new AccessList;
    (*am)[access->ts] = al;
  }

  al->push_back(access);

#ifdef _DEBUG_RACEDETECTOR
  // errs() << "RACEDETECTOR: appending " << (void*)addr << " "
  // << *access << "\n";
#endif
}

void AccessHistory::appendAccess(Access *access) {
  AccessList racy_accesses;

  if(access->isWrite) {
    // remove reads < access->ts
    removeReads(access);
    // remove writes < access->ts
    removeWrites(access);

    // detect write-read races
    if(!reads.empty()) {
      forall(AccessMap, i, reads) {
        // no race if accesses are within the same instruction trunk
        if(i->first != access->ts) {
          AccessList *al = i->second;
          racy_accesses.insert(racy_accesses.end(), al->begin(), al->end());
        }
      }
    }
    // detect write-write races
    if(!writes.empty()) {
      forall(AccessMap, i, writes) {
        // no race if accesses are within the same instruction trunk
        if(i->first != access->ts) {
          AccessList *al = i->second;
          racy_accesses.insert(racy_accesses.end(), al->begin(), al->end());
        }
      }
    }
  } else {// access->isWrite == false
    // remove reads < access->ts
    removeReads(access);

    // detect read-write race
    list<Access*>::const_iterator i;
    forall(AccessMap, i, writes) {
      if(i->first != access->ts // check for same instruction trunk
         && i->first->concurrent(*access->ts)) {
        AccessList *al = i->second;
        racy_accesses.insert(racy_accesses.end(), al->begin(), al->end());
      }
    }
  }

  // add access for real
  appendAccessHelper(access);

  if(!racy_accesses.empty()) {
    access->racy = true;
    racyAccesses.insert(access);
    forall(list<Access*>, i, racy_accesses) {
      Access *racy = *i;
      racy->racy = true;
      racyAccesses.insert(racy);

#ifdef _DEBUG_RACEDETECTOR
      dumpRace(racy, access);
#endif
    }
  }
}

/***/

void RaceDetector::onMemoryAccess(bool isWrite, char *addr, uint8_t data,
                                  const InstTrunk *tr, unsigned idx) {
  AccessHistory *ah;
  AccessMap::iterator ai = accesses.find(addr);
  if(ai == accesses.end()) {
    ah = new AccessHistory(addr);
    accesses[addr] = ah;
  } else {
    ah = ai->second;
  }
  Access *access = new Access(isWrite, data, tr, idx);
  ah->appendAccess(access);
}

unsigned RaceDetector::numRacyAccesses() {
  unsigned n = 0;
  forall(AccessMap, i, accesses) {
    AccessHistory *ah = i->second;
    n += ah->numRacyAccesses();
  }
  return n;
}


typedef pair<InstTrunk*, unsigned> DInstID;

struct DInstEQ {
  bool operator()(const DInstID &di1, const DInstID &di2) const {
    return di1.first == di2.first && di1.second == di2.second;
  }
};

struct DInstHash {
  long operator()(const DInstID &di) const {
    return tr1::hash<void*>()((void*)di.first)
      ^ tr1::hash<unsigned>()(di.second);
  }
};

void RaceDetector::sortRacyAccesses() {

  typedef tr1::unordered_map<DInstID, int, DInstHash> RacyDInstMap;
  typedef map<uint64_t, unsigned>  DataMap;

  forall(AccessMap, i, accesses) {
    AccessHistory *ah = i->second;
    if(ah->racyAccesses.empty())
      continue;

    RacyDInstMap racyDInsts;
    DataMap      dataMap;

    forall(set<Access*>, j, ah->racyAccesses) {
      DInstID di((InstTrunk*)(*j)->ts, (*j)->idx);
      racyDInsts[di] = 1;
    }
    forall(RacyDInstMap, di, racyDInsts) {
      InstTrunk *tr = (*di).first.first;
      unsigned idx = (*di).first.second;
      LInst LI = tr->instLog->getLInst(idx);
      if(LI.getRecType() == StoreRecTy) {
        LStoreInst SI = LInstCast<LStoreInst>(LI);
        DataMap::iterator k = dataMap.find(SI.getData());
        if(k == dataMap.end())
          dataMap[SI.getData()] = 1;
        else
          ++ dataMap[SI.getData()];
      }
    }
    errs() << "writes to " << (void*)ah->addr << ":\n";
    forall(DataMap, di, dataMap) {
      if(di->second == 1) {
        errs() << "unique: " << di->first << "\n";
      } else {
        errs() << "not unique: " << di->first << "\n";
      }
    }
  }

  // match reads with these writes
}

void RaceDetector::dumpRacyAccesses() {
  forall(AccessMap, i, accesses) {
    AccessHistory *ah = i->second;
    if(ah->racyAccesses.empty())
      continue;
    errs() << "RACE on " << (void*)ah->addr << ": ";
    forall(set<Access*>, j, ah->racyAccesses) {
      errs() << **j << ", ";
    }
    errs() << "\n";
  }
}

RaceDetector::RaceDetector() {}

RaceDetector::~RaceDetector() {
  forall(AccessMap, i, accesses) {
    delete i->second;
  }
}

void RaceDetector::install() {
  RaceDetector::the = new RaceDetector();
}

RaceDetector *RaceDetector::the = NULL;


}
