#include "gtest/gtest.h"
#include "recorder/runtime/recruntime.h"

using namespace tern;

TEST(runtime, single_thread) {
  RecorderRT<FCFSScheduler> fcfsRT;
  RecorderRT<RRSchedulerCV> rrcvRT;
  // TODO: unit test cases
}
