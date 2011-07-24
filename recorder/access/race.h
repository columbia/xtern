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

  AccessHistory(void *addr);
  AccessHistory(void *addr, Access *a);
  ~AccessHistory();

  // memory address
  void *addr;

  /// concurrent reads to range [offset, offset+width)
  AccessMap reads;
  /// existing writes to range
  AccessMap writes;
  /// accesses that involve in at least one race
  std::set<Access*>   racyAccesses;
};

class RaceDetector {
public:
  typedef std::tr1::unordered_map<void*, AccessHistory*> AccessMap;

  RaceDetector();
  ~RaceDetector();

  void onMemoryAccess(bool isWrite, char *addr, uint8_t data,
                      const InstTrunk *tr, unsigned idx);
  unsigned numRacyAccesses();
  void sortRacyAccesses();

  void dumpRacyAccesses();

  /// accesses to all memory at byte granularity
  AccessMap  accesses;

  static void install();
  static RaceDetector *the;
};


}

#endif
