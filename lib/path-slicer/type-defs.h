#ifndef __TERN_PATH_SLICER_TYPE_DEFS_H
#define __TERN_PATH_SLICER_TYPE_DEFS_H

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <fstream>

typedef std::pair<long, long> LongPair;
typedef std::pair<long long, long long> LongLongPair;

#define HM_IN(ELEM, SET) (SET.find(ELEM) != SET.end())
#define DM_IN(ELEM, SET) (SET.count(ELEM) > 0)
#define NOT_TAKEN_INSTR "NOT TAKEN"

#endif
