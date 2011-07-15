#include "gtest/gtest.h"
#include "recorder/runtime/recscheduler.h"

using namespace tern;

TEST(scheduler, single_thread) {
  FCFSScheduler fcfs(pthread_self());
  RRSchedulerCV rrcv(pthread_self());
  // TODO: unit test cases
}
