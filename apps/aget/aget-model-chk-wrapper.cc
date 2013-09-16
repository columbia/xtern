#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int client_pid = fork();
  if (client_pid == 0) {
    char *argv[] = { "/home/heming/rcs/smt+mc/xtern/apps/aget/aget",
      "-f", "-p", "8080", "-n2", "http://localhost/run", NULL };  // Heming: download the "run" script, a small file.
    execv("/home/heming/rcs/smt+mc/xtern/apps/aget/aget", argv);
  }
  sleep(0);
  int server_pid = fork();
  if (server_pid == 0) {
    char *argv[] = { "/home/heming/rcs/smt+mc/xtern/apps/mongoose/mg-server",
      "-t", "2", "-r", "/home/heming/rcs/smt+mc/xtern/apps/mongoose", NULL };
    execv("/home/heming/rcs/smt+mc/xtern/apps/mongoose/mg-server", argv);
  }
  waitpid(client_pid, NULL, 0);
  kill(server_pid, SIGTERM);
  waitpid(server_pid, NULL, 0);
  return 0;
}
