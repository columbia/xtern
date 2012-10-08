/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#include "pthread.h"
#include "tern/runtime/scheduler.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <algorithm>
#include "tern/options.h"

using namespace std;
using namespace tern;


Serializer::~Serializer()
{
  fclose(logger);
}

Serializer::Serializer(): 
  TidMap(pthread_self()), turnCount(0) 
{
  logger = fopen("serializer.log", "w");
  assert(logger);
}

unsigned Serializer::incTurnCount(void) { 
  //if ((int) turnCount < INF && idle_done)
    //turnCount = INF;
  int ret = turnCount++;  
  fprintf(logger, "%d %d\n", (int) self(), ret);
  return ret;
}

unsigned Serializer::getTurnCount(void) { 
  //if ((int) turnCount < INF && idle_done)
    //turnCount = INF;
  return turnCount - 1; 
}
