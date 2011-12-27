#include "racy-edge.h"
using namespace tern;

RacyEdge::RacyEdge(DynInstr *prev, DynInstr *post) {
  this->prev = prev;
  this->post = post;
}

RacyEdge::~RacyEdge() {
  
}


