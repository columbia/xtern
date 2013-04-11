#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <pthread.h>
#include "assert.h"

using namespace std;

FILE *pipe_in;
FILE *pipe_out;

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

string next()
{
  FILE *file = pipe_in;
  int len;
  assert(fread((void*) &len, 4, 1, file) == 1);
  char *buffer = new char[len];
  assert((int)fread((void*) buffer, 1, len, file) == len);
  std::string ret = buffer;
//  fprintf(stderr, "received %s\n", buffer);
//  fflush(stderr);
  delete buffer;
  return ret;
}

void request(std::string msg)
{
  FILE *file = pipe_out;
  int len = (int) msg.size() + 1;
//  fprintf(stderr, "sending %d bytes\n", len);
  fwrite((void*) &len, 4, 1, file);
//  fprintf(stderr, "sending %s\n", msg.c_str());
  fwrite((void*) msg.c_str(), 1, len, file);
  fflush(file);
//  fflush(stderr);
}


void usage(int argc, char *argv[])
{
  fprintf(stderr, "Usage:\n\t%s <pipeid>\n", argv[0]);
  exit(1);
}

int main(int argc, char *argv[])
{
  if (argc != 2) usage(argc, argv);

  int pipe_id = atoi(argv[1]);

  pipe_in = fopen(apps_outfile(pipe_id).c_str(), "a+");
  pipe_out = fopen(user_outfile(pipe_id).c_str(), "a+");

  char buffer[256];
  printf("Print Enter to start execution\n");
  fgets(buffer, 256, stdin);
  request("ready");

  while (true)
  {
    printf(">>> ");
    fgets(buffer, 256, stdin);
    buffer[strlen(buffer) - 1] = 0; // remove newline char
    request(buffer);
    string msg = next();
    printf("%s\n\n", msg.c_str());
  }
}

