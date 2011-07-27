/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RACE_DETECTOR_H
#define __TERN_RACE_DETECTOR_H

#include <set>
#include <list>
#include "logaccess.h"

namespace tern {

/// Race-detection metadata for a particular memory access.  We use byte
/// granularity for simplicity.  The old tern implementation has a race
/// detector that tracks accesses at object level.
struct Access {
  Access(bool isWr, uint8_t data, const InstTrunk *tr, unsigned idx);
  Access(const Access& a);

  bool             isWrite;
  uint8_t          data; // value read or written
  const InstTrunk  *ts;  // timestamp of the access
  unsigned         idx;  // index of the instruction that performed the access
  bool             racy; // involved in any race?
};

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const Access& a);

/// History of memory accesses for happens-before based race detection.
/// Based on Dinning and Schonberg's paper "An Empirical Comparison of
/// Monitoring Algorithms for Access Anomaly Detection", we don't have
/// to remember all accesses; instead we need to remember only
/// concurrent reads and one writes to avoid missing races.
///
/// A small tweak is that we want to detect all races for a particular
/// memory location, instead of just any race.  We need all races so that
/// we can totally order all conflicting memory accesses w.r.t the memory
/// location.
struct AccessHistory {

  typedef std::list<Access*>  AccessList;
  typedef std::tr1::unordered_map<const InstTrunk*, AccessList*> AccessMap;
  //typedef std::map<const InstTrunk*, AccessList*> AccessMap;

  static void removeAccesses(Access *a, AccessMap &accesses);

  /// remove reads < access->ts
  void removeReads(Access *access);
  /// remove writes < access->ts; access must be a write
  void removeWrites(Access *access);
  void appendAccess(Access *access);
  void appendAccessHelper(Access *access);

  unsigned numRacyAccesses() { return racyAccesses.size(); }

  void dumpRace(const Access *a1, const Access *a2) const;

  AccessHistory(char *addr);
  AccessHistory(char *addr, Access *a);
  ~AccessHistory();

  // memory address
  char *addr;

  /// concurrent reads to range [offset, offset+width)
  AccessMap reads;
  /// existing writes to range
  AccessMap writes;
  /// accesses that involve in at least one race
  std::set<Access*>   racyAccesses;
};

struct RaceDetector {
  typedef std::tr1::unordered_map<void*, AccessHistory*> AccessMap;

  RaceDetector();
  ~RaceDetector();

  void onMemoryAccess(bool isWrite, char *addr, uint8_t data,
                      const InstTrunk *tr, unsigned idx);
  unsigned numRacyAccesses();
  void sortRacyAccesses(std::list<RacyEdge>& racyEdges);

  void dumpRacyAccesses(); /// for debugging

  /// accesses to all memory at byte granularity
  AccessMap  accesses;

  static void install();
  static RaceDetector *the;
};


struct RaceSorter {
  struct Node;
  struct LTNode;
  struct HRange;
  struct LTRange;
  struct LTEdge;

  typedef std::set<Node*, LTNode>                            NodeSet;
  typedef std::pair<char*, unsigned>                         Range;
  typedef std::map<Range, NodeSet*, LTRange>                 RNMap;
  typedef std::map<unsigned, Node*>                          INMap;
  typedef std::tr1::unordered_map<const InstTrunk*, INMap*> TINMap;

  struct LTInstTrunk {
    bool operator()(const InstTrunk *tr1, const InstTrunk *tr2) {
      return tr1->beginTurn < tr2->beginTurn;
    }
  };

  struct LTNode {
    bool operator()(const Node *n1, const Node *n2) const {
      return n1->ts->beginTurn < n2->ts->beginTurn
        || (n1->ts->beginTurn == n2->ts->beginTurn
            && n1->idx < n2->idx);
    }
  };

  struct HRange{
    size_t operator()(const Range& range) const {
      return std::tr1::hash<void*>()(range.first)
        ^ std::tr1::hash<unsigned>()(range.second);
    }
  };

  struct LTRange {
    bool operator()(const Range &r1, const Range &r2) const {
      return r1.first < r2.first;
    }
  };

  struct Node {
    const InstTrunk  *ts;
    unsigned         idx;
    bool             isWrite;
    char             *addr;
    unsigned         size;
    int64_t          data;
    NodeSet          mergedReads; // only valid if this inst is a write
    NodeSet          inEdges, outEdges;

    bool validInEdge(Node *from);
    bool validOutEdge(Node *to);
    void addInEdge(Node *from);
    void addOutEdge(Node *to);
    void removeInEdge(Node *from);
    void removeOutEdge(Node *to);
    int64_t getDataInRange(const Range& range) const;

    Node(const InstTrunk *ts, unsigned idx, bool isWrite,
         char *addr, unsigned size, uint64_t data);
    ~Node();
  };

  TINMap    tinMap; /// InstTrunk* -> index -> Node map
  RNMap     rnMap;  /// Range -> NodeSet map; nodes with common access range

  void addNode(const InstTrunk *ts, unsigned idx, bool isWrite,
               char *addr, unsigned size, uint64_t data);
  void addNodeFromAccess(Access *access);
  void addEdgesForUniqueWrites(const Range &range, const NodeSet &NS);
  bool topSort(std::list<Node*>& topOrder); /// @topOrder must be an empty list
  bool topSortHelper(Node *node, std::list<Node*>& topOrder,
                     std::tr1::unordered_map<Node*, bool>& visited,
                     std::tr1::unordered_map<Node*, bool>& stack);
  void longestPath(const NodeSet& NS, const std::list<Node*>& topOrder,
                   std::list<Node*>& path);
  void matchReads(Node *write, const NodeSet &reads, const NodeSet &writes);
  bool reachable(Node *from, Node *to);
  void pruneEdges();

  void sortNodes();
  void getRacyEdges(std::list<RacyEdge>& racyEdges);

  void dump();
  void cycleCheck();

  ~RaceSorter();
};

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const RaceSorter::Node &n);

}

#endif
