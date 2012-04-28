#ifndef __TERN_COMMON_CLOCK_MANAGER_H
#define __TERN_COMMON_CLOCK_MANAGER_H

#include "stdint.h"
#include "time.h"
#include "sys/time.h"

namespace tern {

//  following runtime.h to use Java naming style 
//  ClockManager should be protected by locks. This is required for
//  any callers that tend to leverage ClockManager. 
struct ClockManager {
  const static uint64_t factor = 1000000000;
  const static uint64_t INF = -1;

  ClockManager(uint64_t init_clock = 0);

  void tick();
  static void getClock(time_t &t, uint64_t clock);
  static void getClock(timespec &t, uint64_t clock);
  static void getClock(timeval &t, uint64_t clock);

  static uint64_t getTick(const time_t &t) { return (uint64_t) t * factor; }
  static uint64_t getTick(const timespec &t) { return (uint64_t) t.tv_sec * factor + t.tv_nsec; }
  static uint64_t getTick(const timeval &t) { return (uint64_t) t.tv_sec * factor + t.tv_usec * 1000; }

  void reset_rclock();
  uint64_t adjust_timeout(uint64_t timeout); 

  uint64_t epochLength;     //  measured in nanosecond
  uint64_t tickCount;       //  number of calls to tick()
  uint64_t clock;           //  equal to init_clock + tick * epochLen
  uint64_t init_rclock;     //  realtime clock at initialization
  uint64_t next_rclock;     //  realtime clock at initialization
};

}
#endif
