#ifndef __TERN_LOG_ANALYZER_H_
#define __TERN_LOG_ANALYZER_H_

#include <string>

#include "log_reader.h"

struct op_t 
{
  tern::record_t rec;
  int pid;
  int tid;
  std::string get_string(int idx)
  {
    if (idx >= 0 && idx < (int) rec.args.size())
      return rec.args[idx];
    else
      return "";
  }
#define cur_rec rec
  int get_int(int idx)
  {
    if (idx >= 0 && idx < (int) cur_rec.args.size())
    {
      unsigned int x;
      sscanf(cur_rec.args[idx].c_str(), "%x", &x);
      return x; 
    }
    else
      return -1;
  }

  int64_t get_int64(int idx)
  {
    if (idx >= 0 && idx < (int) cur_rec.args.size())
    {
      uint64_t x;
      sscanf(cur_rec.args[idx].c_str(), "%lx", &x);
      return x; 
    }
    else
      return -1;
  }
#undef cur_rec
};



#endif
