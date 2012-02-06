#include "log_reader.h"
#include "tern/syncfuncs.h"

#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>
#include <cstring>

using namespace tern;
using namespace std;

txt_log_reader::txt_log_reader()
  : fin(0)
{
  cur_rec.op = syncfunc::not_sync;
}

txt_log_reader::~txt_log_reader()
{
  if (fin)
    close();
}

bool txt_log_reader::open(const char *filename)
{
  if (fin) 
    close();
  fin = fopen(filename, "r");
  if (fin)
    next();
  return fin;
}

void txt_log_reader::close()
{
  if (!fin) return;
  fclose(fin);
  fin = NULL;
}

unsigned getNameIDFromEvent(string st)
{
  if (st.size() >= 6 && st.substr(st.size() - 6) == "_first")
    return syncfunc::getNameID(st.substr(0, st.size() - 6).c_str());

  if (st.size() >= 7 && st.substr(st.size() - 7) == "_second")
    return syncfunc::getNameID(st.substr(0, st.size() - 7).c_str());

  return syncfunc::getNameID(st.c_str());
}

void txt_log_reader::next()
{
  if (!fin) return;
  buffer[0] = 0;
  if (!fgets(buffer, buffer_size, fin))
  {
    cur_rec.op = syncfunc::not_sync;
    cur_rec.turn = -1;
    cur_rec.args.clear();
    return;
  }

  assert(strlen(buffer) > 5 && "how can you get a record with a short line?");

  istringstream is(buffer);

  string op_name;
  cur_rec.args.clear();

  is >> op_name;
  is >> hex >> cur_rec.insid >> dec;
  is >> cur_rec.turn;

  string st;
  while (is >> st) cur_rec.args.push_back(st); 
  cur_rec.op = getNameIDFromEvent(op_name);
}

bool txt_log_reader::valid()
{
  if (cur_rec.op == syncfunc::not_sync) return false;
  if ((int) cur_rec.turn == -1) return false;

  return true;
}

void txt_log_reader::seek(int idx)
{
  assert(false && "not supported yet");
}

unsigned txt_log_reader::get_op()
{
  return cur_rec.op;
}

unsigned txt_log_reader::get_turn()
{
  return cur_rec.turn;
}

int64_t txt_log_reader::get_int64(int idx)
{
  if (idx >= 0 && idx < (int) cur_rec.args.size())
  {
    int64_t x;
    sscanf(cur_rec.args[idx].c_str(), "%lx", &x);
    return x; 
  }
  else
    return -1;
}

int txt_log_reader::get_int(int idx)
{
  if (idx >= 0 && idx < (int) cur_rec.args.size())
  {
    int x;
    sscanf(cur_rec.args[idx].c_str(), "%x", &x);
    return x; 
  }
  else
    return -1;
}

const char * txt_log_reader::get_str(int idx)
{
  if (idx >= 0 && idx < (int) cur_rec.args.size())
    return cur_rec.args[idx].c_str();
  else
    return NULL;
}

