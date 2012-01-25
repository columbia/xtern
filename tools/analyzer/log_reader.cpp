#include "log_reader.h"
#include "tern/syncfuncs.h"

#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>

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

void txt_log_reader::next()
{
  if (!fin) return;
  fgets(buffer, buffer_size, fin);
  istringstream is(buffer);

  string op_name;
  cur_rec.args.clear();

  is >> op_name;
  is >> cur_rec.turn;

  string st;
  while (is >> st) cur_rec.args.push_back(st); 
  cur_rec.op = syncfunc::getNameID(op_name.c_str());
}

bool txt_log_reader::valid()
{
  if (cur_rec.op == syncfunc::not_sync) return false;

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
 
int txt_log_reader::get_int(int idx)
{
  if (idx >= 0 && idx < (int) cur_rec.args.size())
    return atoi(cur_rec.args[idx].c_str());
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

