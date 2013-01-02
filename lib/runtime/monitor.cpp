#include "tern/runtime/monitor.h"
#include <cstring>
#include "tern/space.h"
#include <pthread.h>
#include <string>
#include "assert.h"
#include <cstdlib>
#include <sys/stat.h>

#include <unistd.h>

using namespace tern;
using namespace std;

void RuntimeMonitor::stop()
{
  if (!running) return;
  running = false;
  send(pipe_in, "quit");
}

void *RuntimeMonitor::monitorThread(void *args)
{
  RuntimeMonitor *monitor = (RuntimeMonitor *)args;

  while (true)
  {
    string cmd = monitor->next();
    if (cmd == "quit")
    {
      fprintf(stderr, "stopping monitor\n");
      monitor->running = false;
      break;
    }

    if (cmd == "help")
    {
      string info =
         "Usage:\n"
         "	help:               show help information\n"
         "	quit:               stop monitor\n"
         ;
      monitor->reply(info);
      continue;
    }

    monitor->reply("unrecognized command");
  }
  return 0;
}

RuntimeMonitor::RuntimeMonitor()
{
  bool backup = Space::setSpace(false); //  call origin

  time_t tt;
  int pipe_id = (int) time(&tt);
  int iret;
  running = true;

  fprintf(stderr, "Creating runtimemontior, pipe_id = %d\n", pipe_id);
  fflush(stderr);

  while (true)
  {
    iret = mkfifo(user_outfile(pipe_id).c_str(), 0666);
    if (iret)
      puts("mkfifo user failed"), sleep(1);
    else break;
  }

  while (true)
  {
    iret = mkfifo(apps_outfile(pipe_id).c_str(), 0666);
    if (iret)
      puts("mkfifo apps failed"), sleep(1);
    else break;
  }

  if (fork() == 0)
  {
    fprintf(stderr, "Running on Child process\n");
    fflush(stderr);

    //  child process
    char buffer[256];
    //sprintf(buffer, "%d", pipe_id);
    sprintf(buffer, "/home/huayang/research/xtern/dync_hook/usermonitor %d", pipe_id);
    unsetenv("LD_PRELOAD");
    //execl("/usr/bin/gnome-terminal", "gnome-terminal", "-x", "sh", "-c", "ls; sleep 10", (char*) 0);
    execl("/usr/bin/gnome-terminal", "gnome-terminal", "-x", "sh", "-c", 
      buffer, (char*) 0);
    //execl("usermonitor", "usermonitor", buffer, (char*) 0);
    exit(0);
  }

  while (true)
  {
    pipe_in = fopen(user_outfile(pipe_id).c_str(), "a+");
    if (!pipe_in)
      printf("failed to open user_output\n"), sleep(1);
    else break;
  }

  while (true)
  {
    pipe_out = fopen(apps_outfile(pipe_id).c_str(), "a+");
    if (!pipe_out)
      printf("failed to open apps_output\n"), sleep(1);
    else break;
  }

  assert(pipe_in && pipe_out);

  string msg = next();
  assert(msg == "ready");

  assert(!pthread_create(&th, NULL, monitorThread, (void*)this));

  Space::setSpace(backup);
}

RuntimeMonitor::~RuntimeMonitor()
{
  bool backup = Space::setSpace(false); //  call origin
  stop();
  fclose(pipe_in);
  fclose(pipe_out);
  Space::setSpace(backup);
}

void RuntimeMonitor::reply(std::string msg)
{
  send(pipe_out, msg);
}

string RuntimeMonitor::recv(FILE *file)
{
/*
  char buffer[256];
  std::string ret;
  while (fgets(buffer, 256, file))
  {
    if (buffer[strlen(buffer) - 1] == '\n') 
    {
      buffer[strlen(buffer) - 1] = 0; //  remove the newline
      ret = ret + buffer;
      break;
    } else
     ret = ret + buffer;
  }
*/
  int len;
  assert(fread((void*) &len, 4, 1, file) == 1);
//  fprintf(stderr, "going to receive %d bytes\n", len);
//  fflush(stderr);
  char *buffer = new char[len];
  assert((int)fread((void*) buffer, 1, len, file) == len);
  std::string ret = buffer;
//  fprintf(stderr, "received %s\n", buffer);
//  fflush(stderr);
  delete buffer;
  return ret;
}

void RuntimeMonitor::send(FILE *file, std::string msg)
{
  int len = (int) msg.size() + 1;
  fwrite((void*) &len, 4, 1, file);
  fwrite((void*) msg.c_str(), 1, len, file);
  fflush(file);
//  fprintf(stderr, "sending %s\n", msg.c_str());
//  fflush(stderr);
}

std::string RuntimeMonitor::next()
{
  return recv(pipe_in);
}

