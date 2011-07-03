#include "gtest/gtest.h"
#include "../../recorder/runtime/log.h"

TEST(recordertest, log) {
  int insid = 33;
  void *addr = (void*)0xdeadbeef;
  long long data = 12345;

  xtern_log_init();
  xtern_log_i64(insid, addr, data);

  EXPECT_EQ(insid, 33);
}
