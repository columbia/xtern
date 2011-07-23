/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RACE_DETECTOR_H
#define __TERN_RACE_DETECTOR_H

#include <list>
#include "logaccess.h"

namespace tern {

/// Race-detection metadata for a particular memory access.  We use byte
/// granularity for simplicity.  The old tern implementation has a race
/// detector that tracks accesses at object level.
struct Access {
  Access(bool isWr, const InstLog::Trunk *tr,
         const InstLog::DInst& inst, uint8_t data);
  Access(const Access& a);

  bool           isWrite;
  const InstLog::Trunk*   ts;        // timestamp of the access
  InstLog::DInst inst;      // instruction that performed the access
  uint8_t        data;      // value read or written
};

/// History of memory accesses for happens-before based race detection.
/// Based on Dinning and Schonberg's paper "An Empirical Comparison of
/// Monitoring Algorithms for Access Anomaly Detection", we don't have
/// to remember all accesses; instead we need to remember only
/// concurrent reads and one writes to avoid missing races.
///
/// A small tweak is that we want to detect all races, instead of just a
/// race for a particular memory location
struct AccessHistory {
  AccessHistory(void *addr);
  AccessHistory(void *addr, Access *a);
  ~AccessHistory();

  static void removeAccesses(Access *a, std::list<Access*>& accesses);

  void printRace(Access *a1, Access *a2);
  /// remove reads < access->ts
  void removeReads(Access *access);
  /// remove writes < access->ts; access must be a write
  void removeWrites(Access *access);
  bool appendAccess(Access *access);

  // memory address
  void *addr;

  /// concurrent reads to range [offset, offset+width)
  std::list<Access*> reads;
  /// existing writes to range
  std::list<Access*> writes;
};

class RaceDetector {
public:
  typedef std::tr1::unordered_map<void*, AccessHistory*> AccessMap;

  RaceDetector();
  ~RaceDetector();

  void onMemoryAccess(InstLog::Trunk *tr, const InstLog::DInst& inst);

  /// byte-granularity accesses to all memory
  AccessMap  accesses;

  static void install();
  static RaceDetector *the;
};


}

#endif
