#include <set>
#include <list>
#include <algorithm>
#include "gtest/gtest.h"
#include "tern/analysis/detect-race.h"

using namespace std;
using namespace tern;

TEST(racedetector, simple) {
  unsigned nracy;
  InstTrunk tr1, tr2, tr3, tr4;
  char *addr = (char*)0xdeadbeef;

  tr1.beginTurn = 0;
  tr1.endTurn   = 2;
  tr2.beginTurn = 1;
  tr2.endTurn   = 3;
  tr3.beginTurn = 3;
  tr3.endTurn   = 5;
  tr4.beginTurn = 4;
  tr4.endTurn   = 6;

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

  // earlier write dominates a read and a write, which race
  raceDetector = new RaceDetector;
  raceDetector->onMemoryAccess(true, addr, 0, &tr1, 0);
  raceDetector->onMemoryAccess(false, addr, 0, &tr3, 0);
  raceDetector->onMemoryAccess(true, addr, 0, &tr4, 5);
  raceDetector->dumpRacyAccesses();
  nracy = raceDetector->numRacyAccesses();
  EXPECT_EQ(2U, nracy);
  delete raceDetector;

  // TODO: earlier read dominates a read and a write, which race

  // TOOD: later write post-dominate a read and a write, which race

  // TODO: later read post-dominate a read and a write, which race
}

struct LTHalf {
  bool operator()(const RacyEdgeHalf &e1, const RacyEdgeHalf &e2) {
    return e1.trunk->beginTurn < e2.trunk->beginTurn
      || (e1.trunk->beginTurn == e2.trunk->beginTurn
          && e1.idx < e2.idx);
  }
};
struct LTEdge {
  bool operator()(const RacyEdge &e1, const RacyEdge &e2) {
    LTHalf lthalf;
    return lthalf(e1.up, e2.up)
      || (!lthalf(e2.up, e1.up) && lthalf(e1.down, e2.down));
  }
};

void sortEdges(list<RacyEdge>& edges) {
  set<RacyEdge, LTEdge> sorted;

  for(list<RacyEdge>::iterator ri=edges.begin(); ri!=edges.end(); ++ri)
    sorted.insert(*ri);

  edges.clear();
  for(set<RacyEdge, LTEdge>::iterator ri=sorted.begin();
      ri!=sorted.end(); ++ri)
    edges.push_back(*ri);
}


TEST(racesorter, simple) {
  RacyEdge edge;
  list<RacyEdge> racyEdges;
  set<RacyEdge, LTEdge> sortedEdges;

  RaceSorter *raceSorter;
  InstTrunk tr1, tr2, tr3, tr4, tr5;
  unsigned idx1 = 100, idx2 = 200, idx3 = 300;
  char *addr1 = (char*)0xdeadbeef, *addr2 = (char*)0xbeefdead;
  uint64_t data1 = 12345, data2 = 67890;
  unsigned size1 = 4, size2 = 8;

  //  tr1  || tr2
  //  tr1  <  tr3
  //  tr1  || tr4
  //  tr1  <  tr5
  //  tr2  <  tr3
  //  tr2  || tr4
  //  tr2  || tr5
  //  tr3  || tr4
  //  tr3  || tr5
  //  tr4  || tr5
  tr1.beginTurn = 0;
  tr1.endTurn   = 3;
  tr2.beginTurn = 1;
  tr2.endTurn   = 4;
  tr3.beginTurn = 4;
  tr3.endTurn   = 5;
  tr4.beginTurn = 2;
  tr4.endTurn   = 5;
  tr5.beginTurn = 3;
  tr5.endTurn   = 5;

  // one edge
  // store addr1 data1 \.
  //                    \ load addr1 = data1
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,  true, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2, false, addr1, size1, data1);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(1U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  delete raceSorter;

  // one edge
  // store addr1 data1 \.
  //                    \ load addr1 = data1 \ (hb-edge from InstTrunk)
  //                                          \ store addr1 data2
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,  true, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2, false, addr1, size1, data1);
  raceSorter->addNode(&tr3, idx3,  true, addr1, size1, data2);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(1U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  delete raceSorter;

  // two edges, cross
  // store addr1 data1   \/ store addr2 data2
  // load  addr2 = data1 /\ load  addr1 data1
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1, true, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx1+1, false, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2-1, true, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2, false, addr1, size1, data1);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(2U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  edge = racyEdges.back();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2-1, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx1+1, edge.down.idx);
  delete raceSorter;

  // two edges
  // store addr1 data1 \.
  //                     load addr1 = data1
  // store addr1 data2 /
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,  true, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx3,  true, addr1, size1, data2);
  raceSorter->addNode(&tr2, idx2, false, addr1, size1, data1);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(2U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  edge = racyEdges.back();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx3, edge.down.idx);
  delete raceSorter;

  // two edges, one irrelevant write
  // store addr1 data1 \.
  // store addr2 data2
  //                     load addr1 = data1
  // store addr1 data2 /
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,  true, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx1+1,  true, addr2, size2, data2);
  raceSorter->addNode(&tr1, idx3,  true, addr1, size1, data2);
  raceSorter->addNode(&tr2, idx2, false, addr1, size1, data1);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(2U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  edge = racyEdges.back();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx3, edge.down.idx);
  delete raceSorter;

  // three edges
  // store addr1 data1 \.
  //                   / load addr1 = data1
  // store addr1 data2
  //                   \load addr1 = data2
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1-1, true, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx1,   true, addr1, size1, data2);
  raceSorter->addNode(&tr2, idx2,   false, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2+1, false, addr1, size1, data2);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(3U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1-1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2+1, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx1, edge.down.idx);
  delete raceSorter;

  // one edge, subsume
  // store addr1 data2
  // store addr1 data1 \.
  //                    \ load addr1 = data1
  //                      load addr1 = data1
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1-1, true, addr1, size1, data2);
  raceSorter->addNode(&tr1, idx1,   true, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2,   false, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2+1, false, addr1, size1, data1);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(1U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  delete raceSorter;

  // one edge, subsume
  // store addr1 data1
  // store addr2 data2 \.
  //                    \ load addr2 = data2
  //                      load addr1 = data1
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1-1, true, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx1,   true, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2,   false, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2+1, false, addr1, size1, data1);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(1U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  delete raceSorter;

  // one edge, concurrent write
  //                    /-------------------- store addr1 data2
  // store addr1 data1 \.
  //                    \ load addr1 = data1
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,  true, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2, false, addr1, size1, data1);
  raceSorter->addNode(&tr4, idx3,  true, addr1, size1, data2);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(2U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  edge = racyEdges.back();
  EXPECT_EQ(&tr4, edge.up.trunk);
  EXPECT_EQ(idx3, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx1, edge.down.idx);
  delete raceSorter;

  // three edges, concurrent write
  //                   /-------------------- store addr1 data2
  // store addr1 data1                      /
  //                   \load addr1 = data1 /
  //                    load addr1 = data2/
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,   true, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2,   false, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2+1, false, addr1, size1, data2);
  raceSorter->addNode(&tr4, idx3,   true, addr1, size1, data2);
  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);
  EXPECT_EQ(3U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr4, edge.down.trunk);
  EXPECT_EQ(idx3, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr4, edge.up.trunk);
  EXPECT_EQ(idx3, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2+1, edge.down.idx);
  delete raceSorter;

  // three edges, non-unique writes
  // store addr1 data1
  // store addr2 data2---------------------\.
  //                 \  store addr2, data1  \.
  //                  \-load addr2 = data2   \.
  //                    load addr1 = data1    \.
  //                    store addr2, data1  ---\-store addr2 data2
  //                    load addr2 = data2----/
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1,   true, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx1+1, true, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2-2, true, addr2, size2, data1);
  raceSorter->addNode(&tr2, idx2-1, false, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2,   false, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2+1, true, addr2, size2, data1);
  raceSorter->addNode(&tr2, idx2+2, false, addr2, size2, data2);
  raceSorter->addNode(&tr5, idx3,   true, addr2, size2, data2);

  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);

  EXPECT_EQ(4U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1+1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2-1, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2-2, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx1+1, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2+1, edge.up.idx);
  EXPECT_EQ(&tr5, edge.down.trunk);
  EXPECT_EQ(idx3, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr5, edge.up.trunk);
  EXPECT_EQ(idx3, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2+2, edge.down.idx);
  delete raceSorter;

  // three edges, non-unique writes
  //                     /----------------- store addr1 data1
  // load addr1 = data1-/
  // store addr2  data2-\.
  //                     \-load addr2 data2
  //                      -store addr1 data1
  //                     /-store addr1 data2
  // load addr1 = data2-/
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1-1, false, addr1, size1, data1);
  raceSorter->addNode(&tr1, idx1,   true, addr2, size2, data2);
  raceSorter->addNode(&tr1, idx1+1, false, addr1, size1, data2);
  raceSorter->addNode(&tr2, idx2,   false, addr2, size2, data2);
  raceSorter->addNode(&tr2, idx2+1, true, addr1, size1, data1);
  raceSorter->addNode(&tr2, idx2+2, true, addr1, size1, data2);
  raceSorter->addNode(&tr4, idx3,   true, addr1, size1, data1);

  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);

  EXPECT_EQ(4U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr1, edge.up.trunk);
  EXPECT_EQ(idx1, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr2, edge.up.trunk);
  EXPECT_EQ(idx2+2, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx1+1, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr4, edge.up.trunk);
  EXPECT_EQ(idx3, edge.up.idx);
  EXPECT_EQ(&tr1, edge.down.trunk);
  EXPECT_EQ(idx1-1, edge.down.idx);
  racyEdges.pop_front();
  edge = racyEdges.front();
  EXPECT_EQ(&tr4, edge.up.trunk);
  EXPECT_EQ(idx3, edge.up.idx);
  EXPECT_EQ(&tr2, edge.down.trunk);
  EXPECT_EQ(idx2+1, edge.down.idx);
  delete raceSorter;

  // three edges, non-unique writes
  // store addr1 data1 (happens-before other writes)
  //                                  /store addr1 data2
  //                store addr1 data1-
  //                                  \load addr1 = data1
  raceSorter = new RaceSorter;
  raceSorter->addNode(&tr1, idx1, true, addr1, size1, data1);
  raceSorter->addNode(&tr3, idx2, true, addr1, size1, data1);
  raceSorter->addNode(&tr5, idx3, true, addr1, size1, data2);
  raceSorter->addNode(&tr5, idx3+1, false, addr1, size1, data1);

  raceSorter->sortNodes();
  raceSorter->getRacyEdges(racyEdges);
  sortEdges(racyEdges);

  EXPECT_EQ(2U, racyEdges.size());
  edge = racyEdges.front();
  EXPECT_EQ(&tr5, edge.up.trunk);
  EXPECT_EQ(idx3, edge.up.idx);
  EXPECT_EQ(&tr3, edge.down.trunk);
  EXPECT_EQ(idx2, edge.down.idx);
  edge = racyEdges.back();
  EXPECT_EQ(&tr3, edge.up.trunk);
  EXPECT_EQ(idx2, edge.up.idx);
  EXPECT_EQ(&tr5, edge.down.trunk);
  EXPECT_EQ(idx3+1, edge.down.idx);
  delete raceSorter;
}
