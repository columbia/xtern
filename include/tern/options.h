#ifndef __TERN_COMMON_OPTIONS_H
#define __TERN_COMMON_OPTIONS_H

#include <string>

namespace options
{

void get_value(const char *name, std::string &ret);
void get_value(const char *name, int &ret);

template<class T>
T get(const char *name)
{
  T ret;
  get_value(name, ret);
  return ret;
}

bool read_options(const char *filename);

}

#endif
