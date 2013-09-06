#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int client_pid = fork();
  if (client_pid == 0) {
    char *argv[] = { "/home/heming/rcs/smt+mc/xtern/apps/ldap/slapd-mtread", "-H", "ldap://localhost:9011/", "-D", 
                     "cn=Manager,dc=example,dc=com", "-w", "secret", "-e", "cn=Monitor", "-c", "2", "-m", "2", "-L", "5", "-l", "10", NULL };
    execv("/home/heming/rcs/smt+mc/xtern/apps/ldap/slapd-mtread", argv);
  }
  sleep(0);
  int server_pid = fork();
  if (server_pid == 0) {
    char *argv[] = { "/home/heming/rcs/smt+mc/xtern/apps/ldap/slapd", "-s0", "-f", "slapd.1.conf", "-h",
      "ldap://localhost:9011/", "-d", "stats", NULL };
    execv("/home/heming/rcs/smt+mc/xtern/apps/ldap/slapd", argv);
  }
  waitpid(client_pid, NULL, 0);
  kill(server_pid, SIGTERM);
  waitpid(server_pid, NULL, 0);
  return 0;
}

