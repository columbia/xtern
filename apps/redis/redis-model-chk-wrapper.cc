#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int client_pid = fork();
  if (client_pid == 0) {
    //char *argv[] = { "/home/heming/rcs/smt+mc/xtern/apps/redis/redis-benchmark", "-n", "2", "-c", "2", "-t", "get" , NULL };
    char *argv[] = { "/home/jsimsa/smt+mc/xtern/apps/redis/redis-benchmark", "-n", "2", "-t", "lrange_100" , NULL };
    execv("/home/heming/rcs/smt+mc/xtern/apps/redis/redis-benchmark", argv);
  }
  sleep(0);
  int server_pid = fork();
  if (server_pid == 0) {
    char *argv[] = { "/home/heming/rcs/smt+mc/xtern/apps/redis/redis-server", NULL };
    execv("/home/heming/rcs/smt+mc/xtern/apps/redis/redis-server", argv);
  }
  waitpid(client_pid, NULL, 0);
  kill(server_pid, SIGTERM);
  waitpid(server_pid, NULL, 0);
  return 0;
}

