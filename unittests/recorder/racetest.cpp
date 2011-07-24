#include "gtest/gtest.h"
#include "recorder/access/race.h"

using namespace tern;

TEST(racedetector, simple) {
  unsigned nracy;
  InstTrunk tr1, tr2, tr3;
  char *addr = (char*)0xdeadbeef;

  tr1.beginTurn = 0;
  tr1.endTurn   = 2;
  tr2.beginTurn = 1;
  tr2.endTurn   = 3;
  tr3.beginTurn = 3;
  tr3.endTurn   = 5;

  RaceDetector *raceDetector;

  // no race; both are read
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(false, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(false, addr, 0, &tr2, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(0U, nracy);
  delete raceDetector;

  // no race; different byte
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(true, addr+1, 0, &tr2, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(0U, nracy);
  delete raceDetector;

  // no race; happens-before
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr3, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(0U, nracy);
  delete raceDetector;

  // no race; same thread
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(0U, nracy);
  delete raceDetector;

  // no race; same thread
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(false, addr, 0, &tr1, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(0U, nracy);
  delete raceDetector;

  // two racy accesses from a read-write race
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr2, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(2U, nracy);
  delete raceDetector;

  // two racy accesses from a write-write race
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(false, addr, 0, &tr2, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(2U, nracy);
  delete raceDetector;

  // three racy accesses from two read-write races
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(false, addr, 0, &tr2, 0);
  raceDetector->onMemoryAccess(false, addr, 0, &tr2, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(3U, nracy);
  delete raceDetector;

  // three racy accesses from two read-write races
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(false, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr2, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr2, 0);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(3U, nracy);
  delete raceDetector;
}
