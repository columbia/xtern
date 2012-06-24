#include <map>
#include <vector>
#include <set>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>

#include "analyzer.h"
#include "tern/syncfuncs.h"

using namespace std;
using namespace tern;

extern bool po_signature; //  declared in analyzer.cpp. the corresponding cmd line option is -po
typedef pair<int, int> id_pair; 
typedef map<id_pair, vector<op_t> > thread_log_map;
typedef thread_log_map::iterator tlmi;

void split_log(vector<op_t> &x, thread_log_map &xtm)
{
  for (unsigned i = 0; i < x.size(); ++i) 
  { 
    op_t &op = x[i]; 
    id_pair id = make_pair(op.pid, op.rec.tid); 
    xtm[id].push_back(op); 
  }
}

bool try_match(vector<op_t> &x, vector<op_t> &y)
{
  if (x.size() && y.size() 
    && x.front().rec.tid != y.front().rec.tid)
    return false;
  
  int n = (int) min(x.size(), y.size());
  
  for (int i = 0; i < n; ++i)
  {
#define comp(item)\
    if (x[i].item != y[i].item) return false;
    comp(rec.tid);
    comp(rec.op);
    //comp(rec.insid);  //  doesn't work for dync loaded libraries, e.g. bdb

    //  maybe arguments?
#undef comp
  }
  return true;
}

void comp_log_po(
  vector<op_t> &x, const char *dir1, vector<op_t> &y, const char *dir2)
{
}

void comp_log(
  vector<op_t> &x, const char *dir1, vector<op_t> &y, const char *dir2)
{
/*  if (po_signature)
  {
    comp_log_po(x, dir1, y, dir2);
    return; 
  }
*/
  thread_log_map xtm, ytm;
  split_log(x, xtm);
  split_log(y, ytm);

  bool result = true;
  for (tlmi it = xtm.begin(); it != xtm.end(); ++it)
  {
    bool matched = false;
    for (tlmi jt = ytm.begin(); !matched && jt != ytm.end(); ++jt)
    {
      if (it->first.second != jt->first.second) 
        continue; //  tid must equal
      matched |= try_match(it->second, jt->second);
    }
    if (!matched)
    {
      fprintf(stderr, "<%d %d> in %s is not matched to any thread in %s\n",
        it->first.first, it->first.second, dir1, dir2);
      result = false;
    }
  }
  if (result)
  {
    printf("%s bipre-matches %s\n", dir1, dir2);
  } else 
    printf("%s bipre-dismatches %s\n", dir1, dir2);
}
