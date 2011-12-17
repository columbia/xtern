#include "tern/options.h"
#include <map>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <boost/algorithm/string.hpp>

using namespace std;

namespace options
{

typedef std::map<std::string, std::string> data_t;
data_t data;

bool read_options(const char *filename)
{
  FILE *file = fopen(filename, "r");
  const int len = 1024;
  static char buffer[len];
  if (!file) goto read_failed;
  buffer[len - 1] = 0;  //  make sure length limit.
  while (fgets(buffer, len - 1, file))
  {
    char* p = strchr(buffer, '=');
    if (!p) continue;
    string name(buffer, p);
    string value(p + 1, buffer + strlen(buffer));
    boost::trim(name);
    boost::trim(value);
    if (data.find(name) != data.end())
    {
      fprintf(stderr, "warning: multiple option defined for %s\n", name.c_str());
      fflush(stderr);
    }
    if (name != "")
      data[name] = value;
  }
  return true;
read_failed:
  fprintf(stderr, "read options failed! please make sure %s exists\n", filename);
  fflush(stderr);
  fclose(file);
  return false;
}

void get_value(const char *name, std::string &ret)
{
  data_t::iterator it = data.find(name);
  if (it == data.end())
  {
    fprintf(stderr, "warning: %s is not defined in options\n", name);
    fflush(stderr);
    ret = "";
  }
  ret = it->second;
}

void get_value(const char *name, int &ret)
{
  std::string &st = data[name];
  ret = atoi(st.c_str());
}
/*
void get_value(const char *name, ...)
{
}
*/

//template<class T>
//T get(const char *name)

}


