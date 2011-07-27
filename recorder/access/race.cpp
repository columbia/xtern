/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <queue>
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
  : isWrite(isWr), data(d), ts(c), idx(index), racy(false) { }

Access::Access(const Access &a)
  : isWrite(a.isWrite), data(a.data), ts(a.ts), idx(a.idx), racy(false) { }

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
    delete i->second;
  }
  forall(AccessMap, i, writes) {
    forall(AccessList, j, *(i->second)) {
      if(!(*j)->racy)
        delete *j;
    }
    delete i->second;
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

  forall(AccessMap, i, accesses)
    if(i->first->happensBefore(*a->ts))
      toErase.push_back(i->first);

  forall(list<const InstTrunk*>, i, toErase) {
    AccessMap::iterator ai = accesses.find(*i);
    AccessList *al = ai->second;
    accesses.erase(ai);

#ifdef _DEBUG_RACEDETECTOR
    errs() << "RACEDETECTOR: removing accesses for " << **i << "\n";
#endif

    forall(AccessList, j, *al)
      if(!(*j)->racy) // otherwise, this access is owned by @racyAccesses
        delete *j;
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

// FIXME: can use a range-based race detector if it is faster
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

void RaceDetector::sortRacyAccesses(list<RacyEdge>& racyEdges) {

  RaceSorter raceSorter;

  forall(AccessMap, i, accesses) {
    AccessHistory *ah = i->second;
    if(ah->racyAccesses.empty())
      continue;
    // FIXME 1: get boundary accesses (the read or write access that
    // immediate-dominate all racy accesses, and read node that
    // post-immediate-dominate all racy accesses if no write node
    // post-immediate-dominate all racy accesses
    //
    // FIXME 2: should split racy accesses into groups, where accesses
    // within one group are concurrent at least with one other access in
    // the group, but all accesses in one group must happen-before (or
    // after) another.
    forall(set<Access*>, j, ah->racyAccesses)
      raceSorter.addNodeFromAccess(*j);
  }

  raceSorter.sortNodes();
  raceSorter.getRacyEdges(racyEdges);
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

void RaceSorter::getRacyEdges(list<RacyEdge>& racyEdges) {
  racyEdges.clear();

  forall(TINMap, tini, tinMap) {
    forall(INMap, ini, *tini->second) {
      Node *from = ini->second;
      forall(NodeSet, ni, from->outEdges) {
        Node *to = *ni;
        if(from->ts == to->ts || from->ts->happensBefore(*to->ts))
          continue;
        racyEdges.push_back(RacyEdge(from->ts, from->idx, to->ts, to->idx));
      }
    }
  }
}

void RaceSorter::sortNodes() {
  // key observation: most racy writes are unique (reads may not be)
  //
  // data structure
  // ts -> ts, idx -> Node {ts, idx, isWrite, addr, size, data}
  //
  // create edges based on turn
  //     ts1, idx1 -> ts2, idx2 if ts1 < ts2 or ts1 = ts2 && idx1 < idx2
  //
  // merge nodes based on unique writes
  //     for each address range
  //        for each write touching this address range
  //           if no other write of same value
  //             merge read of addr of this unique value with write node
  //               [node has a list of merged reads]
  //
  // Note: doing it at byte granularity may miss things.  think of
  // two-byte writes x1 x2; x1 x3; x4 x2; x4 x3.  each write is unique at
  // two-byte granularity, but not unique at byte granularity.  Thus, we
  // do it at instruction granularity
  //
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
  //
  //   Note: must also check boundary conditions (the value at addr before
  //   all these racy accesses, and value of addr after all racy accesses
  //   must match with the selected write order)
  //
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

  map<Range, NodeSet, LTRange> crnMap; // range -> concurrent writes
  forall(RNMap, rni, rnMap) {
    NodeSet orderedWrites, allWrites, concurrentWrites;

    // find all writes from the set of accesses to the current range
    forall(NodeSet, ni, *rni->second) {
      if((*ni)->isWrite)
        allWrites.insert(*ni);
    }

    // find writes that are ordered along the longest path
    list<Node*> path;
    longestPath(allWrites, topOrder, path);
    forall(list<Node*>, ni, path) {
      // need to check if the write is in @allWrites as well, since @path
      // may have writes on other ranges
      if((*ni)->isWrite && allWrites.find(*ni)!=allWrites.end())
        orderedWrites.insert(*ni);
    }

    // find concurrent writes
    set_difference(allWrites.begin(), allWrites.end(),
                   orderedWrites.begin(), orderedWrites.end(),
                   insert_iterator<NodeSet>(concurrentWrites,
                                            concurrentWrites.begin()));
    if(concurrentWrites.size() > 0)
      crnMap[rni->first] = concurrentWrites;
  }

  // slow path: search where to put these concurrent writes
  for(map<Range, NodeSet, LTRange>::iterator crni=crnMap.begin();
      crni!=crnMap.end(); ++crni) {
    errs() << "concurrent writes for " << (void*)crni->first.first << ": ";
    forall(NodeSet, ni, crni->second)
      errs() << **ni << ", ";
    errs() << "\n";
  }

#if 0
  errs() << "number all writes: " << allWrites.size() << ", "
         << "concurrent writes for " << (void*) rni->first.first <<": ";
  forall(NodeSet, ni, concurrentWrites)
    errs() << *ni << ", ";
  assert(0 && "some writes are still concurrent; need slow search!");
#endif

  // prune subsumed edges
  pruneEdges();
}

/// longest path in an unweighted DAG
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
    if(NS.find(node) != NS.end())
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
  typedef tr1::unordered_map<uint64_t, Node*>    DataWriteMap;
  typedef tr1::unordered_map<uint64_t, NodeSet>  DataReadMap;

  // discover writes with unique values
  DataWriteMap dataWriteMap;
  DataReadMap  dataReadMap;
  forall(NodeSet, ni, NS) {
    Node *node = *ni;
    uint64_t data = node->getDataInRange(range);
    if(!node->isWrite) { // read
      dataReadMap[data].insert(node);
      continue;
    }
    // write
    DataWriteMap::iterator di = dataWriteMap.find(data);
    if(di == dataWriteMap.end())
      dataWriteMap[data] = node;
    else
      di->second = NULL; // not unique
  }

  // collect all writes, used in matchReads()
  NodeSet writes;
  forall(NodeSet, ni, NS)
    if((*ni)->isWrite)
      writes.insert(*ni);

  // match unique writes with corresponding reads
  forall(DataWriteMap, di, dataWriteMap) {
    if(!di->second)
      continue;
    // unique write
    Node *write = di->second;
#ifdef _DEBUG_RACEDETECTOR
    errs() << "unique write " << write->data << "\n";
#endif
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

  topOrder.clear();

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
    forall(INMap, ini, *tini->second)
      delete ini->second;
    delete tini->second;
  }
  forall(RNMap, rni, rnMap)
    delete rni->second;
}

void RaceSorter::dump() {
  forall(TINMap, tini, tinMap) {
    forall(INMap, ini, *tini->second)
      errs() << *ini->second;
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

    // add @write -> @read
    write->addOutEdge(read);
    write->mergedReads.insert(read);

    // if @awrite -> @read, add @awrite -> @write
    forall(NodeSet, ni, writes) {
      Node *awrite = *ni;
      if(write == awrite) // no self cycle
        continue;
      if(read->validInEdge(awrite))
        write->addInEdge(awrite);
    }

    // if @write -> @awrite, add @read -> @awrite
    forall(NodeSet, ni, writes) {
      Node *awrite = *ni;
      if(write == awrite) // no self cycle
        continue;
      if(write->validOutEdge(awrite))
        read->addOutEdge(awrite);
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

// we only look at InstTrunk pairs, thus may still leave redundant edges
// in the happens-before graph.  For instance, consider edges:
//
//   (ts1, 5) -> (ts2, 10), (ts2, 10) -> (ts3, 20), and (ts1, 4) -> (ts3, 21)
//
// the third edge is redundant, but our algorithm wouldn't prune it
//
// FIXME: use longestPath(), and prune edges not along the longest path
void RaceSorter::pruneEdges() {
  typedef pair<Node*, Node*> Edge;
  typedef map<unsigned, Edge> INNMap;
  typedef tr1::unordered_map<const InstTrunk*, INNMap> TINNMap;

  forall(TINMap, tini, tinMap) {
    // @earliest tracks the earliest indexes in other InstTrunks that
    // happen-after nodes in current InstTrunk pointed to by @tini->second
    TINNMap earliest;

    forall(INMap, ini, *tini->second) {
      Node *from = ini->second;

      // self pruning of edges from the same node
      tr1::unordered_map<const InstTrunk*, Node*> nodeEarliest;
      list<Node*> toErase;
      forall(NodeSet, ni, from->outEdges) {
        Node *to = *ni;
        tr1::unordered_map<const InstTrunk*, Node*>::iterator ei;
        ei = nodeEarliest.find(to->ts);
        if(ei == nodeEarliest.end())
          nodeEarliest[to->ts] = to;
        else {
          Node *oldto = ei->second;
          if(oldto->idx < to->idx)
            toErase.push_back(to);
          else
            toErase.push_back(oldto);
        }
      }
      forall(list<Node*>, ni, toErase)
        from->removeOutEdge(*ni);

      // pruning old edges from the same InstTrunk
      forall(NodeSet, ni, from->outEdges) {
        Node *to = *ni;
        if(from->ts->happensBefore(*to->ts) || from->ts == to->ts)
          continue;
        // self pruning
        TINNMap::iterator tinni = earliest.find(to->ts);
        INNMap &innMap = earliest[to->ts];
        INNMap::iterator inni = innMap.lower_bound(to->idx);
        while(inni != innMap.end()) {
          // remove edges that from-to subsumes
          INNMap::iterator prv = inni ++;
          Edge &edge = prv->second;
          Node *oldfrom = edge.first;
          Node *oldto   = edge.second;
          oldfrom->removeOutEdge(oldto);
          innMap.erase(prv);
        }
        innMap[to->idx] = Edge(from, to);
      }
    }
  }
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
  return data & ((1ULL<<(end*8)) - (1ULL<<(start*8)));
}

bool RaceSorter::Node::validInEdge(Node *from) {
  assert(this != from);
  return inEdges.find(from) != inEdges.end();
}

bool RaceSorter::Node::validOutEdge(Node *to) {
  return to->validInEdge(this);
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

void RaceSorter::Node::removeInEdge(Node *from) {
#ifdef _DEBUG_RACEDETECTOR
  errs() << "RACESORTER: removing edge " << from << " to " << this << "\n";
#endif
  assert(this != from);
  assert(this->inEdges.find(from)  != this->inEdges.end());
  assert(from->outEdges.find(this) != from->outEdges.end());
  this->inEdges.erase(from);
  from->outEdges.erase(this);
}

void RaceSorter::Node::removeOutEdge(Node *to) {
  to->removeInEdge(this);
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

raw_ostream &operator<< (raw_ostream &o, const RaceSorter::Node &node) {
  o << "Node " << &node << ": " << *node.ts << ", "
    << node.idx << ", " << (node.isWrite?"write":"read") << ", "
    << (void*)node.addr << ", " << node.data << ", "
    << node.size << " bytes\n";
  if(!node.inEdges.empty()) {
    o << "  in :";
    forall(RaceSorter::NodeSet, ni, node.inEdges) {
      o << " " << *ni;
    }
    o << "\n";
  }
  if(!node.outEdges.empty()) {
    o << "  out:";
    forall(RaceSorter::NodeSet, ni, node.outEdges) {
      o << " " << *ni << " ";
    }
    o << "\n";
  }
  return o;
}

}
