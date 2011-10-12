/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <queue>
#include "common/util.h"
#include "tern/analysis/detect-race.h"

// #define _DEBUG_RACEDETECTOR

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
           << "ts=[" << a.ts->beginTurn << "," << a.ts->endTurn << ") "
           << "idx=" << a.idx;
}

/***/

AccessHistory::AccessHistory(char *address)
  : addr(address), lastWrite(NULL), preDominator(NULL), postDominator(NULL) {}

AccessHistory::AccessHistory(char *address, Access *a) {
  addr = address;
  lastWrite = NULL;
  preDominator = NULL;
  postDominator = NULL;
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
      if((*j)->racy)
        continue;
      if(*j == lastWrite)
        continue;
      delete *j;
    }
    delete i->second;
  }
  forall(set<Access*>,  i, racyAccesses)
    delete *i;
  if(lastWrite)
    delete lastWrite;
  if(preDominator)
    delete preDominator;
  if(postDominator)
    delete postDominator;
}

void AccessHistory::removeReads(Access *access) {
  removeAccesses(access, reads);
}

void AccessHistory::removeWrites(Access *access) {
  removeAccesses(access, writes);
}


/// to sort races, we need to match reads with writes.  However, it is
/// possible for a read to happen before a racing write, therefore reading
/// the value last written by a previous, non-racing write.  This function
/// tracks such writes so that RaceSorter can detect these reads
void AccessHistory::updateLastWrite(const std::list<const InstTrunk*> &toErase){
  Access *lastWriteCandidate = NULL;
  forallconst(list<const InstTrunk*>, i, toErase) {
    AccessMap::iterator ai = writes.find(*i);
    AccessList *al = ai->second;
    forall(AccessList, j, *al) {
      Access *write = *j;
      if(lastWriteCandidate == NULL)
        lastWriteCandidate = write;
      else {
        if(lastWriteCandidate->ts->endTurn < write->ts->endTurn
           || (lastWriteCandidate->ts->endTurn == write->ts->endTurn
               && lastWriteCandidate->idx < write->idx))
          lastWriteCandidate = write;
      }
    }
  }
  if(lastWriteCandidate) {
    if(lastWrite)
      delete lastWrite;
    lastWrite = lastWriteCandidate;
  }
}

void AccessHistory::removeAccesses(Access *a, AccessMap &accesses) {

  list<const InstTrunk*> toErase;

  forall(AccessMap, i, accesses)
    if(i->first->happensBefore(*a->ts))
      toErase.push_back(i->first);

  // update lastWrite if necessary
  if(&accesses == &writes && racyAccesses.empty())
    updateLastWrite(toErase);

  forall(list<const InstTrunk*>, i, toErase) {
    AccessMap::iterator ai = accesses.find(*i);
    AccessList *al = ai->second;
    accesses.erase(ai);

#ifdef _DEBUG_RACEDETECTOR
    errs() << "RACEDETECTOR: removing accesses for " << **i << "\n";
#endif

    forall(AccessList, j, *al) {
      if((*j)->racy) //this access is owned by @racyAccesses
        continue;
      if(*j == lastWrite) // need lastWrite for a little while
        continue;
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

void AccessHistory::markRacy(Access *access) {
  access->racy = true;
  racyAccesses.insert(access);
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
    markRacy(access);
    forall(list<Access*>, i, racy_accesses) {
      Access *racy = *i;
      markRacy(racy);

      // add last write that dominates all future accesses, so that we can
      // sort races
      if(lastWrite) {
        preDominator = lastWrite;
        lastWrite = NULL;
      }

#ifdef _DEBUG_RACEDETECTOR
      dumpRace(racy, access);
#endif
    }
  } else {
    // no race.
    // TODO: first access that post-dominates all racy accesses
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

    if(ah->preDominator)
      raceSorter.addNodeFromAccess(ah->preDominator);

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

#ifdef _DEBUG_RACEDETECTOR
  errs() << "all racy edges:\n";
  for(list<RacyEdge>::iterator ei = racyEdges.begin();
      ei != racyEdges.end(); ++ei)
    errs() << *ei << "\n";
#endif
}

struct LTPR {
  bool operator()(const pair<RaceSorter::Range, RaceSorter::Node *>& p1,
                  const pair<RaceSorter::Range, RaceSorter::Node *>& p2) const{
    return p1.second->mayMatch.size() < p2.second->mayMatch.size();
  }
};

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

  verify();

#ifdef _DEBUG_RACEDETECTOR
  dump();
#endif

  list<pair<Range, Node*> > pendingNodes;

  // find concurrent writes
  forall(RNMap, rni, rnMap) {
    NodeSet orderedWrites, concurrentWrites;

    // find writes that are ordered along the longest path
    list<Node *> path;
    longestWritePath(rni->first, path);
    copy(path.begin(), path.end(),
         insert_iterator<NodeSet>(orderedWrites, orderedWrites.begin()));

    // find all writes from the set of accesses to the current range
    forall(NodeSet, ni, *rni->second) {
      if((*ni)->isWrite)
        concurrentWrites.insert(*ni);
    }

    // find concurrent writes
    setDiff(concurrentWrites, orderedWrites);
    if(concurrentWrites.size() > 0) {
      forall(NodeSet, cni, concurrentWrites)
        pendingNodes.push_back(pair<Range, Node*>(rni->first, *cni));
    }
  }

  // find unresolved reads
  list<pair<Range, Node *> > pendingReads;
  forall(RNMap, rni, rnMap) {
    forall(NodeSet, ni, *rni->second) {
      Node *read = *ni;
      if(!read->isWrite && read->match.empty())
        pendingReads.push_back(pair<Range, Node*>(rni->first, read));
    }
  }

  // pendingReads.sort(LTPR());
  pendingNodes.insert(pendingNodes.end(),
                      pendingReads.begin(), pendingReads.end());

#ifdef _DEBUG_RACEDETECTOR
  errs() << "start search for " << pendingNodes.size() << " nodes:\n";
  for(list<pair<Range, Node*> >::iterator pi = pendingNodes.begin();
      pi != pendingNodes.end(); ++pi) {
    errs() << *pi->second << "\n";
  }
#endif

  bool found = search(pendingNodes);

  assert(found);

#ifdef _DEBUG
  verify();
#endif

  // prune subsumed edges
  pruneEdges();

#ifdef _DEBUG
  verify();
#endif
}

void RaceSorter::addWriteWriteEdge(Node *w1, Node *w2,
                                   const NNSMap &R,
                                   list<pair<Node*, Node*> > &undos) {
#ifdef _DEBUG_RACEDETECTOR
  errs() << "add write " << w1 << " before " << w2 << "\n";
#endif

  // @w1 -> @w2
  if(!isReachable(w1, w2, R)) {
    w2->addInEdge(w1);
    undos.push_back(pair<Node*, Node*>(w1, w2));
  }

  // matching read of @w1 -> @w2
  forall(NodeSet, ni, w1->match) {
    Node *read = *ni;
    if(!isReachable(read, w2, R)) {
      read->addOutEdge(w2);
      undos.push_back(pair<Node*, Node*>(read, w2));
    }
  }
}

bool RaceSorter::hasCycle() {
  list<Node*> topOrder;
  return topSort(topOrder);
}

void RaceSorter::undo(const list<std::pair<Node*, Node*> > &undos) {
  for(list<pair<Node*, Node*> >::const_iterator ui = undos.begin();
      ui != undos.end(); ++ ui) {
    Node *from = ui->first;
    Node *to = ui->second;
    from->removeOutEdge(to);
  }
}

/// find a position to put @write
bool RaceSorter::searchForWrite(Node *write,
                                const list<Node*> &path,
                                const NNSMap &R,
                                list<pair<Range, Node*> > &pendingNodes) {
  list<Node*>::const_iterator pi = path.begin();

  for(;;) {
    if(pi != path.end() && isReachable(*pi, write, R)) {
      ++ pi;
      continue;
    }

#ifdef _DEBUG_RACEDETECTOR
    errs() << "trying to insert " << write << " before ";
    if(pi != path.end())
      errs() << *pi << " in ";
    else
      errs() << "end() in ";
    forallconst(list<Node*>, i, path)
      errs() << *i << ", ";
    errs() << "\n";
#endif

    list<pair<Node*, Node*> > undos;

    if(pi != path.end())
      addWriteWriteEdge(write, *pi, R, undos);

    if(pi != path.begin()) {
      list<Node*>::const_iterator prv = pi;
      -- prv;
      addWriteWriteEdge(*prv, write, R, undos);
    }

    if(!hasCycle()) {
      // NOTE: we don't just insert @write to @path to get the new longest
      // write path.  The reason is that once a write is added, we can
      // grow the longest path by more than just the write added.  For
      // instance, think of appending a write at the end of the current
      // longest path.  If the write has a HB edge to another write, then
      // we grow the longest write path by two.
      if(search(pendingNodes))
        return true;
    }

#ifdef _DEBUG_RACEDETECTOR
    errs() << "can't insert " << write << " before ";
    if(pi != path.end())
      errs() << *pi << "\n";
    else
      errs() << "end()\n";
#endif

    // undo edges
    undo(undos);

    if(pi == path.end() || isReachable(write, *pi, R))
      break;

    ++ pi;
  }
  return false;
}


bool RaceSorter::searchForRead(Node *read,
                               const list<Node*> &path,
                               const NNSMap &R,
                               list<pair<Range, Node*> > &pendingNodes) {

  assert(!path.empty() && !read->mayMatch.empty()
         && "read of initial values (i.e., w/o stores) is not handled!");

  list<Node*>::const_iterator pi = path.begin();
  ++ pi;

  for(;;) {
    if(pi != path.end() && isReachable(*pi, read, R)) {
      ++ pi;
      continue;
    }

    // FIXME: either read data section from ELF, or use the values from
    // earlier non-racing reads
    // assert(pi != path.begin()
    //       && "read of initial values (i.e., w/o stores) is not handled!");

    list<Node*>::const_iterator prv = pi;
    -- prv;
    Node *write = *prv;
    // check if previous write is a may-match
    if(read->mayMatch.find(write) == read->mayMatch.end()) {
      if(pi == path.end())
        return false;
      ++ pi;
      continue;
    }

    // check if previous write already happens-after us
    if(isReachable(read, write, R))
      return false;

#ifdef _DEBUG_RACEDETECTOR
    errs() << "trying to insert " << read << " after " << write << " in ";
    forallconst(list<Node*>, i, path)
      errs() << *i << ", ";
    errs() << "\n";
#endif

    list<pair<Node*, Node*> > undos;

    // add write -> read edge
    if(!isReachable(write, read, R)) {
      write->addOutEdge(read);
      undos.push_back(pair<Node*, Node*>(write, read));
    }

    // add read -> next write edge
    if(pi != path.end()) {
      Node *nextWrite = *pi;
      if(!isReachable(read, nextWrite, R)) {
        read->addOutEdge(nextWrite);
        undos.push_back(pair<Node*, Node*>(read, nextWrite));
      }
    }

    if(!hasCycle()) {
      // pass @path down instead of recomputing it in next search()
      // because @path won't change (we've done ordering the writes).
      if(search(pendingNodes, path))
        return true;
    }

#ifdef _DEBUG_RACEDETECTOR
    errs() << "can't insert " << read << " after " << write << "\n";
#endif

    // undo edges
    undo(undos);

    if(pi == path.end())
      break;

    ++ pi;
  }

  return false;
}

bool RaceSorter::search(list<pair<Range, Node*> > &pendingNodes) {

  if(pendingNodes.empty())
    return true;

  list<Node*> writePath;
  pair<Range, Node*> RN = pendingNodes.front();
  longestWritePath(RN.first, writePath);

  return search(pendingNodes, writePath);
}

bool RaceSorter::search(list<pair<Range, Node*> > &pendingNodes,
                        const list<Node*> &path) {

  if(pendingNodes.empty())
    return true;

  pair<Range, Node*> RN = pendingNodes.front();
  pendingNodes.pop_front();

  NNSMap R;
  reachable(R, true);

#ifdef _DEBUG_RACEDETECTOR
  errs() << "search " << *RN.second << "; "
         << pendingNodes.size() << "remain \n";
#endif

  bool found;
  if(RN.second->isWrite)
    found = searchForWrite(RN.second, path, R, pendingNodes);
  else
    found = searchForRead(RN.second, path, R, pendingNodes);

#ifdef _DEBUG_RACEDETECTOR
  errs() << "backtrack " << *RN.second << "\n";
#endif

  if(!found)
    pendingNodes.push_front(RN);
  return found;
}

/// find writes that are ordered along the longest path
void RaceSorter::longestWritePath(const Range &range, list<Node*>& path) {

  NodeSet &NS = *rnMap[range];
  NodeSet allWrites;

  forall(NodeSet, ni, NS)
    if((*ni)->isWrite)
      allWrites.insert(*ni);

  list<Node*> topOrder;
  topSort(topOrder);

  longestPath(allWrites, topOrder, path);

  list<Node*>::iterator prv, cur;
  for(cur=path.begin(); cur != path.end();) {
    prv = cur ++;
    if(!(*prv)->isWrite || !(*prv)->include(range))
      path.erase(prv);
  }
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

#ifdef _DEBUG_RACESORTER
  NodeSet visited;
  forall(list<Node*>, pi, path) {
    Node *node = *pi;
    assert(visited.find(node) == visited.end());
    visited.insert(node);
  }
#endif
}

void RaceSorter::addEdgesForUniqueWrites(const Range &range,
                                         const NodeSet &NS) {
  typedef tr1::unordered_map<uint64_t, NodeSet>  DataMap;

  // discover writes with unique values
  DataMap dataWriteMap;
  DataMap dataReadMap;
  forall(NodeSet, ni, NS) {
    Node *node = *ni;
    uint64_t data = node->getDataInRange(range);
    if(!node->isWrite) { // read
      dataReadMap[data].insert(node);
      continue;
    }
    // write
    dataWriteMap[data].insert(node);
  }

  // collect all writes, used in matchReads()
  NodeSet writes;
  forall(NodeSet, ni, NS)
    if((*ni)->isWrite)
      writes.insert(*ni);

  // match unique writes with corresponding reads
  forall(DataMap, wi, dataWriteMap) {
    if(wi->second.size() > 1) {
      DataMap::iterator ri = dataReadMap.find(wi->first);
      if(ri != dataReadMap.end())
        matchWritesReads(wi->second, ri->second);
      continue;
    }
    // unique write
    Node *write = *wi->second.begin();
#ifdef _DEBUG_RACEDETECTOR
    errs() << "unique write " << write->data << ", "
           << wi->first << "\n";
    if(dataReadMap.find(wi->first) == dataReadMap.end())
      errs() << "but value not observed\n";
#endif
    DataMap::iterator dri = dataReadMap.find(wi->first);
    if(dri == dataReadMap.end())
      continue;
    matchUniqueWriteReads(write, dri->second);

#ifdef _DEBUG_RACEDETECTOR
    verify();
#endif
  }

  /// find nodes in @writes that happens before @reads, and add
  /// happens-before edges from these writes to @write.  also find nodes
  /// in @writes that happens after @write, and add happens-before edges
  /// from @reads to these nodes.
  NNSMap prevWrites, nextWrites;
  immediateWrites(range, prevWrites, true);
  immediateWrites(range, nextWrites, false);

  forall(DataMap, di, dataWriteMap) {
    if(di->second.size() > 1)
      continue;
    Node *write = *di->second.begin();
    forall(NodeSet, ri, write->match) {
      Node *read = *ri;
      // if @awrite -> @read, add @awrite -> @write
      forall(NodeSet, ni, prevWrites[read]) {
        Node *awrite = *ni;
        if(write == awrite) // no self cycle
          continue;
#ifdef _DEBUG_RACEDETECTOR
        errs() << "awrite->write\n";
#endif
        write->addInEdge(awrite);
      }
      // if @write -> @awrite, add @read -> @awrite
      forall(NodeSet, ni, nextWrites[write]) {
        Node *awrite = *ni;
        if(write == awrite) // no self cycle
          continue;
#ifdef _DEBUG_RACEDETECTOR
        errs() << "read -> awrite\n";
#endif
        read->addOutEdge(awrite);
      }
    }
  }
}

// can be made pretty using templates (set_union or set_difference would
// be a template argument)
void RaceSorter::setUnion(NodeSet& NS1, const NodeSet &NS2) {
  NodeSet temp;
  temp.swap(NS1);
  // NOTE: it's important to use the same comparison functor LTNode as how
  // we defined NodeSet here.  If we don't do so, STL would use the
  // set_union with "<" as the comparison operator, which lead to weird
  // results.  Why is STL so stupid and not to generate a type error if
  // these comparison operators don't match?
  set_union(temp.begin(), temp.end(), NS2.begin(), NS2.end(),
            insert_iterator<NodeSet>(NS1, NS1.begin()), LTNode());
}

void RaceSorter::setDiff(NodeSet& NS1, const NodeSet &NS2) {
  NodeSet temp;
  temp.swap(NS1);
  set_difference(temp.begin(), temp.end(), NS2.begin(), NS2.end(),
                 insert_iterator<NodeSet>(NS1, NS1.begin()), LTNode());
}

void RaceSorter::immediateWrites(const Range &range,
                                 NNSMap& writes,
                                 bool isForward) {
  list<Node*> topOrder;
  NNSMap killedIn;
  NNSMap immediateIn;
  NNSMap killedOut;
  NNSMap immediateOut;

  topSort(topOrder);

  if(!isForward)
    topOrder.reverse();

  forall(list<Node*>, ti, topOrder) {
    Node *node = *ti;
    NodeSet &prev = isForward? node->inEdges : node->outEdges;

    // immediateIn  = union (immediateOut of prv) - union (killedOut of prv)
    // killedIn     = union (killedOut of prv)
    // immediateOut = if write, write; else immediateIn
    // killedOut    = if write, immediateIn U killedIn; else killedIn
    NodeSet immediateUion, killedUion;
    forall(NodeSet, ni, prev) {
      setUnion(immediateIn[node], immediateOut[*ni]);
      setUnion(killedIn[node], killedOut[*ni]);
    }
    setDiff(immediateIn[node], killedIn[node]);

    if(node->isWrite && node->include(range)) {
      immediateOut[node].insert(node);
      killedOut[node] = immediateIn[node];
      setUnion(killedOut[node], killedIn[node]);
    } else {
      immediateOut[node] = immediateIn[node];
      killedOut[node] = killedIn[node];
    }
  }
  writes.swap(immediateIn);
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

void RaceSorter::matchWritesReads(NodeSet &writes, NodeSet &reads) {

#ifdef _DEBUG_RACEDETECTOR
  errs() << "RACESORTER: match writes ";
  forall(NodeSet, wi, writes)
    errs() << *wi << ", ";
  errs() << " with reads ";
  forall(NodeSet, ri, writes)
    errs() << *ri << ", ";
  errs() << "\n";
#endif

  forall(NodeSet, wi, writes)
    (*wi)->addMayMatch(reads);
}

/// add happens-before edges to from @write to @reads.
void RaceSorter::matchUniqueWriteReads(Node *write, NodeSet &reads) {
  assert(write->isWrite);

  forall(NodeSet, ni, reads) {
    Node *read = *ni;
    assert(!read->isWrite);

#ifdef _DEBUG_RACEDETECTOR
    errs() << "RACESORTER: match write " << write
           << " with read " << read << "\n";
#endif

    // even though write and read may be in same trunk, we must still
    // match them here.
    //
    // add @write -> @read
    write->addOutEdge(read);
    write->addMatch(read);
  }
}

bool RaceSorter::isReachable(Node *from, Node *to, const NNSMap& R) {
  NNSMap::const_iterator nsi =  R.find(to);
  assert(nsi != R.end());
  const NodeSet &NS = nsi->second;
  return NS.find(from) != NS.end();
}

void RaceSorter::reachable(NNSMap& reachable,
                           bool isForward) {
  list<Node*> topOrder;
  topSort(topOrder);

  if(!isForward)
    topOrder.reverse();

  forall(list<Node*>, ti, topOrder) {
    Node *node = *ti;
    NodeSet &prev = isForward? node->inEdges : node->outEdges;

    // reachable[node] = union reachable[prev], {node}
    forall(NodeSet, ni, prev)
      setUnion(reachable[node], reachable[*ni]);
    reachable[node].insert(node);
  }
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

void RaceSorter::verify() {
  if(hasCycle()) {
    dump();
    assert(0 && "happens-before graph can't have a cycle!");
  }

#if 0
  forall(RNMap, rni, rnMap) {
    list<Node*> path;
    NodeSet allW, orderedW;

    longestWritePath(rni->first, path);
    copy(path.begin(), path.end(),
         insert_iterator<NodeSet>(orderedW, orderedW.begin()));
    forall(NodeSet, ni, *rni->second)
      if((*ni)->isWrite)
        allW.insert(*ni);
    if(allW.size() == orderedW.size()) {
      // TODO: check that read/write data match
    }
  }
#endif
}

/***/

int64_t RaceSorter::Node::getDataInRange(const Range& range) const{
  unsigned start = 8 * (range.first - addr);
  unsigned end   = 8 * (range.first + range.second - addr);
  uint64_t mask;

  assert(end <= 8*sizeof(data));

  // WTH?  1ULL << 64 = 1 ?
  if(end == 8*sizeof(data))
    mask = 0;
  else
    mask = (1ULL<<end);
  mask -= 1ULL << start;

  return data & mask;
}

bool RaceSorter::Node::include(const Range& range) const{
  return (addr <= range.first) && ((addr+size) >= (range.first+range.second));
}

bool RaceSorter::Node::overlap(const Node* other) const {
  if(other->addr + other->size <= addr)
    return false;
  if(addr + size <= other->addr)
    return false;
  return true;
}

bool RaceSorter::Node::hasInEdge(Node *from) {
  assert(this != from);
  return inEdges.find(from) != inEdges.end();
}

bool RaceSorter::Node::hasOutEdge(Node *to) {
  return to->hasInEdge(this);
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

void RaceSorter::Node::addMatch(Node *read) {
  this->match.insert(read);
  read->match.insert(this);
}

void RaceSorter::Node::addMayMatch(NodeSet& reads) {
  setUnion(this->mayMatch, reads);
  forall(NodeSet, ri, reads)
    (*ri)->mayMatch.insert(this);
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
  if(!node.match.empty()) {
    o << "  match:";
    forall(RaceSorter::NodeSet, ni, node.match) {
      o << " " << *ni << " ";
    }
    o << "\n";
  }
  if(!node.mayMatch.empty()) {
    o << "  mayMatch:";
    forall(RaceSorter::NodeSet, ni, node.mayMatch) {
      o << " " << *ni << " ";
    }
    o << "\n";
  }
  return o;
}

}
