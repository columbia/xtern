#ifndef __MONITOR_H_
#define __MONITOR_H_

#include <string>
#include "stdio.h"

inline std::string user_outfile(int id)
{
  char buffer[256];
  sprintf(buffer, "/tmp/user_out_%d", id);
  return buffer;
}

inline std::string apps_outfile(int id)
{
  char buffer[256];
  sprintf(buffer, "/tmp/apps_out_%d", id);
  return buffer;
}

namespace tern {

/*
class PipeBase
{
protected:
  std::string recv(FILE *file);
  void send(FILE *file, std::string msg);
  FILE *connect(std::string filename);
};
*/
class RuntimeMonitor
{
public:
  RuntimeMonitor();
  ~RuntimeMonitor();
  void stop();

protected:
  void reply(std::string msg);
  void send(FILE *file, std::string msg);
  std::string recv(FILE *file);
  std::string next();

  FILE *pipe_in;
  FILE *pipe_out;
  pthread_t th;
  bool running;

  static void *monitorThread(void *args);
};

}

#endif
