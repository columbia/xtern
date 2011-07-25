/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <queue>
#include "race.h"
#include "util.h"

//#define _DEBUG_RACEDETECTOR

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

AccessHistory::AccessHistory(char *address): addr(address) {}

AccessHistory::AccessHistory(char *address, Access *a) {
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
    errs() << "RACEDETECTOR: removing accesses for " << **i << "\n";
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
   errs() << "RACEDETECTOR: appending " << (void*)addr << " "
          << *access << "\n";
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

  RaceSorter raceSorter;

  forall(AccessMap, i, accesses) {
    AccessHistory *ah = i->second;
    if(ah->racyAccesses.empty())
      continue;
    forall(set<Access*>, j, ah->racyAccesses) {
      raceSorter.addNodeFromAccess(*j);
    }
  }
  raceSorter.sortNodes();
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

/***/

void RaceSorter::addNodeFromAccess(Access *access) {
  InstLog *log = access->ts->instLog;
  LInst LI = log->getLInst(access->idx);
  const LMemInst& MI = LInstCast<LMemInst>(LI);

  addNode(access->ts, access->idx, access->isWrite,
          MI.getAddr(), MI.getDataSize(), MI.getData());
}

void RaceSorter::addNode(const InstTrunk *ts, unsigned idx, bool isWrite,
                         char *addr, unsigned size, uint64_t data) {
  INMap *inMap;
  TINMap::iterator tini = tinMap.find(ts);
  if(tini != tinMap.end()) {
    inMap = tinMap[ts];
  } else {
    inMap = new INMap;
    tinMap[ts] = inMap;
  }

  INMap::iterator ini = inMap->find(idx);
  if(ini != inMap->end())
    return;

  Node *node = new Node(ts, idx, isWrite, addr, size, data);
  (*inMap)[idx] = node;

  // add node to range->node set map
  NodeSet *nodeSet;
  Range range(node->addr, node->size);
  RNMap::iterator rni = rnMap.find(range);

  if(rni == rnMap.end()) {
    // create new node set
    nodeSet = new NodeSet;
    rnMap[range] = nodeSet;
  } else if(rni->first == range) {
    // same range
    nodeSet = rni->second;
  } else {
    // split range
    assert(0 && "split range not implemented yet!\n");
  }
  nodeSet->insert(node);
}

void RaceSorter::sortNodes() {
  // key observation: most racy writes are unique (reads may not be)
  //
  // data structure
  // ts -> ts, idx -> Node {ts, idx, isWrite, addr, size, data}

  // create edges based on turn
  //     ts1, idx1 -> ts2, idx2 if ts1 < ts2 or ts1 = ts2 && idx1 < idx2

  // merge nodes based on unique writes
  //     for each address range
  //        for each write touching this address range
  //           if no other write of same value
  //             merge read of addr of this unique value with write node
  //               [node has a list of merged reads]

  // Note: doing it at byte granularity may miss things.  think of
  // two-byte writes x1 x2; x1 x3; x4 x2; x4 x3.  each write is unique at
  // two-byte granularity, but not unique at byte granularity.  Thus, we
  // do it at instruction granularity

  // detect whether there are still concurrent writes
  //
  //   find path with maximum number of writes for a common byte range
  //   (top sort)
  //   other writes to same addr, but not on this path are concurrent
  //
  // for each write in concurrent writes
  //   find possible positions (paths between nodes that have edges to
  //        this node, and nodes that have edges from this node)
  //   for each position, insert
  //   repeat with next write
  //   if can't find a position, backtrack
  //   if can't match reads to writes, backtrack

  //   Note: must also check boundary conditions (the value at addr before
  //   all these racy accesses, and value of addr after all racy accesses
  //   must match with the selected write order)

  // result: partial order of read/write instructions (total order for
  // instructions that access a common range of bytes)


  // add happens-before edges based on InstTrunk (TurnRange)
  forall(TINMap, tini1, tinMap) {
    forall(TINMap, tini2, tinMap) {
      if(tini1->first->happensBefore(*tini2->first)) {
        // last node from tini1
        INMap::reverse_iterator ini1 = tini1->second->rbegin();
        // first node from tini2
        INMap::iterator ini2 = tini2->second->begin();
        assert(ini1 != tini1->second->rend()
               && ini2 != tini2->second->end());
        ini1->second->addOutEdge(ini2->second);
      }
    }
  }

  // add happens-before edges based on index
  forall(TINMap, tini, tinMap) {
    INMap *inMap = tini->second;
    if(inMap->size() > 1) {
      INMap::iterator cur, nxt;
      nxt = cur = inMap->begin();
      ++ nxt;
      for(; nxt!=inMap->end();++cur, ++nxt)
        cur->second->addOutEdge(nxt->second);
    }
  }

  // add happens-before edges for unique writes and corresponding reads
  forall(RNMap, rni, rnMap) {
    const Range &range = rni->first;
    const NodeSet &NS = *rni->second;
    addEdgesForUniqueWrites(range, NS);
  }

#ifdef _DEBUG_RACEDETECTOR
  dump();
#endif

  // find writes that are still concurrent
  list<Node*> topOrder;
  bool hasCycle = topSort(topOrder);
  assert(!hasCycle);

#ifdef _DEBUG_RACEDETECTOR
  errs() << "top order: ";
  forall(list<Node*>, ni, topOrder) {
    errs() << *ni << ", ";
  }
  errs() << "\n";
#endif

  forall(RNMap, rni, rnMap) {
    list<Node*> path;
    longestPath(*rni->second, topOrder, path);

    NodeSet orderedWrites, allWrites, concurrentWrites;
    forall(list<Node*>, ni, path) {
      if((*ni)->isWrite)
        orderedWrites.insert(*ni);
    }
    forall(NodeSet, ni, *rni->second) {
      if((*ni)->isWrite)
        allWrites.insert(*ni);
    }
    set_difference(allWrites.begin(), allWrites.end(),
                   orderedWrites.begin(), orderedWrites.end(),
                   insert_iterator<NodeSet>(concurrentWrites,
                                            concurrentWrites.begin()));
    if(concurrentWrites.size() > 0) {
      errs() << "concurrent writes for " << (void*) rni->first.first <<": ";
      forall(NodeSet, ni, concurrentWrites) {
        errs() << *ni << ", ";
      }
      assert(0 && "some writes are still concurrent; need slow search!");
    }
  }
}

void RaceSorter::longestPath(const NodeSet& NS, const list<Node*>& topOrder,
                             list<Node*>& path) {
  typedef tr1::unordered_map<Node*, pair<unsigned, Node*> > PathMap;

  unsigned maxOverall = 0;
  Node *maxOverallNode = NULL;

  PathMap pathTo;
  for(list<Node*>::const_iterator ci=topOrder.begin();
      ci!=topOrder.end(); ++ci){
    Node *node = *ci;
    unsigned max = 0;
    Node *maxPrev = NULL;
    for(NodeSet::iterator ni=node->inEdges.begin();
        ni!=node->inEdges.end(); ++ni) {
      Node *prev = *ni;
      assert(pathTo.find(prev) != pathTo.end());
      if(pathTo[prev].first >= max) {
        max = pathTo[prev].first;
        maxPrev = prev;
      }
    }
    if(node->isWrite && NS.find(node) != NS.end())
      ++ max;
    pathTo[node].first  = max;
    pathTo[node].second = maxPrev;

    if(max >= maxOverall) {
      maxOverall = max;
      maxOverallNode = node;
    }
  }

  assert(maxOverallNode);
  Node *node = maxOverallNode;
  do {
    path.push_front(node);
    node = pathTo[node].second;
  } while(node);
}

void RaceSorter::addEdgesForUniqueWrites(const Range &range,
                                         const NodeSet &NS) {
  typedef tr1::unordered_map<uint64_t, Node*>        DataMap;
  typedef tr1::unordered_map<uint64_t, set<Node*> >  DataReadMap;

  // discover writes with unique values
  NodeSet     writes;
  DataMap     dataMap;
  DataReadMap dataReadMap;
  forall(NodeSet, ni, NS) {
    Node *node = *ni;
    uint64_t data = node->getDataInRange(range);
    if(!node->isWrite) { // read
      dataReadMap[data].insert(node);
      continue;
    }
    // write
    DataMap::iterator di = dataMap.find(data);
    if(di == dataMap.end())
      dataMap[data] = node;
    else
      di->second = NULL; // not unique
    writes.insert(node);
  }

  // match unique writes with corresponding reads
  forall(DataMap, di, dataMap) {
    if(!di->second)
      continue;
    // unique write
    Node *write = di->second;
    DataReadMap::iterator dri = dataReadMap.find(di->first);
    if(dri == dataReadMap.end())
      continue;
    matchReads(write, dri->second, writes);
#ifdef _DEBUG
    cycleCheck();
#endif
  }
}

bool RaceSorter::topSort(list<Node*>& topOrder) {
  std::list<Node*> roots;
  forall(TINMap, tini, tinMap) {
    forall(INMap, ini, *tini->second) {
      Node *node = ini->second;
      if(node->inEdges.empty())
        roots.push_back(node);
    }
  }
  bool ret = false;
  tr1::unordered_map<Node*, bool> visited;
  tr1::unordered_map<Node*, bool> stack;
  if(roots.empty() && !tinMap.empty())
    return true;
  forall(list<Node*>, ri, roots) {
    ret |= topSortHelper(*ri, topOrder, visited, stack);
  }
  unsigned nNode = 0;
  forall(TINMap, tini, tinMap) {
    nNode += tini->second->size();
  }
  ret |= (nNode != topOrder.size());
  return ret;
}

bool RaceSorter::topSortHelper(Node *node, std::list<Node*>& topOrder,
                        std::tr1::unordered_map<Node*, bool>& visited,
                        std::tr1::unordered_map<Node*, bool>& stack) {

  if(stack.find(node) != stack.end())
    return true; // cycle detected

  if(visited.find(node) != visited.end())
    return false;

  stack[node]  = true;
  visited[node] = true;

  bool ret = false;
  forall(NodeSet, ni, node->outEdges) {
    ret |= topSortHelper(*ni, topOrder, visited, stack);
  }

  stack.erase(node);
  topOrder.push_front(node);
  return ret;
}

RaceSorter::~RaceSorter() {
  forall(TINMap, tini, tinMap) {
    forall(INMap, ini, *tini->second) {
      delete ini->second;
    }
  }
  forall(RNMap, rni, rnMap) {
    delete rni->second;
  }
}

void RaceSorter::dump() {
  forall(TINMap, tini, tinMap) {
    forall(INMap, ini, *tini->second) {
      Node *node = ini->second;
      errs() << "Node " << node << ": " << *node->ts << ", "
             << node->idx << ", " << (node->isWrite?"write":"read") << ", "
             << (void*)node->addr << ", " << node->data << "\n";
      if(!node->inEdges.empty()) {
        errs() << "  in :";
        forall(NodeSet, ni, node->inEdges) {
          errs() << " " << *ni;
        }
        errs() << "\n";
      }
      if(!node->outEdges.empty()) {
        errs() << "  out:";
        forall(NodeSet, ni, node->outEdges) {
          errs() << " " << *ni << " ";
        }
        errs() << "\n";
      }
    }
  }
}

/// add happens-before edges to @reads; find @writes that happens before
/// @reads, and add happens-before edges from these writes to this write
void RaceSorter::matchReads(Node *write, const NodeSet &reads,
                            const NodeSet &writes) {
  assert(write->isWrite);
  forall(NodeSet, ni, reads) {
    Node *read = *ni;
    assert(!read->isWrite);
    if(read->ts == write->ts) // already have happens-before edges
      continue;

#ifdef _DEBUG_RACEDETECTOR
    errs() << "RACESORTER: match write " << write
           << " with read " << read << "\n";
#endif

    // add edge from this write to @read
    write->addOutEdge(read);
    write->mergedReads.insert(read);

    // add edges from writes that happen-before @read to this write
    forall(NodeSet, ni, writes) {
      Node *awrite = *ni;
      if(write == awrite) // no self cycle
        continue;
      if(reachable(awrite, read))
        write->addInEdge(awrite);
    }
  }
}

bool RaceSorter::reachable(Node *from, Node *to) {
  // fast path: happens-before from TurnRange or log index
  if(from->ts->happensBefore(*to->ts))
    return true;
  if(from->ts == to->ts)
    return from->idx < to->idx;

  // slow path: breadth-first search
  std::queue<Node*> q;
  q.push(from);
  while(!q.empty()) {
    Node *cur = q.front();
    q.pop();
    if(cur == to)
      return true;
    forall(NodeSet, ni, cur->outEdges)
      q.push(*ni);
  }
  return false;
}

void RaceSorter::cycleCheck() {
  list<Node*> topOrder;
  if(topSort(topOrder)) {
    dump();
    assert(0 && "happens-before graph can't have a cycle!");
  }
}

/***/

int64_t RaceSorter::Node::getDataInRange(const Range& range) const{
  unsigned start = range.first - addr;
  unsigned end   = range.first + range.second - addr;
  uint64_t data  = data & (1<<end - 1<<start);
  return data;
}

void RaceSorter::Node::addInEdge(Node *from) {
#ifdef _DEBUG_RACEDETECTOR
  errs() << "RACESORTER: adding edge " << from << " to " << this << "\n";
#endif
  assert(this != from);
  this->inEdges.insert(from);
  from->outEdges.insert(this);
}

void RaceSorter::Node::addOutEdge(Node *to) {
  to->addInEdge(this);
}

RaceSorter::Node::Node(const InstTrunk *ts, unsigned idx, bool isWrite,
                       char *addr, unsigned size, uint64_t data) {
  this->ts      = ts;
  this->idx     = idx;
  this->isWrite = isWrite;
  this->addr    = addr;
  this->size    = size;
  this->data    = data;
}

RaceSorter::Node::~Node() { }

}
