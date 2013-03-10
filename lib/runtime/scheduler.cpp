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
#include <sys/stat.h>
#include "tern/options.h"

using namespace std;
using namespace tern;


Serializer::~Serializer()
{
  if (options::log_sync)
    fclose(logger);
}

Serializer::Serializer(): 
  TidMap(pthread_self()), turnCount(0) 
{
  if (options::log_sync) {
    mkdir(options::output_dir.c_str(), 0777);
    std::string logPath = options::output_dir + "/serializer.log";
    logger = fopen(logPath.c_str(), "w");
    assert(logger);
  }
}

unsigned Serializer::incTurnCount(void) { 
  int ret = turnCount++;  
  if (options::log_sync)
    fprintf(logger, "%d %d\n", (int) self(), ret);
  return ret;
}

unsigned Serializer::getTurnCount(void) { 
  return turnCount - 1; 
}
