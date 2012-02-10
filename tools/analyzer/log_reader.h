#ifndef __TERN_LOG_READER_
#define __TERN_LOG_READER_

//#include "tern/syncfuncs.h"
#include <vector>
#include <string>
#include <cstring>
#include <deque>
#include <stdint.h>

namespace tern
{

struct record_t
{
  unsigned insid;
  unsigned op;
  unsigned turn; 
  std::vector<std::string> args;
};

class log_reader
{
public:
  virtual bool open(const char *filename) = 0;
  virtual void close() = 0;
  virtual void next() = 0;
  virtual bool valid() = 0;
  virtual void seek(int idx) = 0;

  //  get data from current record.
  virtual unsigned get_op() = 0; 
  virtual unsigned get_turn() = 0; 
  virtual int get_int(int idx) = 0;
  virtual int64_t get_int64(int idx) = 0;
  virtual const char * get_str(int idx) = 0;
  virtual record_t get_current_rec() = 0;
};

class txt_log_reader : public log_reader
{
public:
  txt_log_reader();
  ~txt_log_reader();
  virtual bool open(const char *filename);
  virtual void close();
  virtual void next();
  virtual bool valid();
  virtual void seek(int idx);

  //  get data from current record.
  virtual unsigned get_op(); 
  virtual unsigned get_turn();
  virtual int get_int(int idx);
  virtual int64_t get_int64(int idx);
  virtual const char * get_str(int idx);
  virtual record_t get_current_rec() { return cur_rec; }

protected:
  FILE *fin;
  static const int buffer_size = 1024;
  char buffer[buffer_size];
  record_t cur_rec;
};

}

#endif
