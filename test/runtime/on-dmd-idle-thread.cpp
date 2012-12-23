// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime" -ternannotlib "%ternannotlib"  -ternbcruntime "%ternbcruntime" -nondet

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>

#define N (100)

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

void *client_thread(void *arg)
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    portno = *((int*)arg);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname("127.0.0.1");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    // We do not care about the logic of this program, we only care about the schedule of 
    // idle thread is on demand.
    for (int i = 0; i < 2; i++)
      connect(0,(struct sockaddr *) &serv_addr,sizeof(serv_addr));

    for (int i = 0; i < 10; i++) {
      pthread_mutex_lock(&mutex);
      pthread_mutex_unlock(&mutex);
    }
    printf("Done with lock/unlock\n");
    return NULL;
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     pthread_t th;
     int optval = 1;

     srand(time(NULL));
     portno = 20000 + rand()%1000;
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0)
        error("ERROR opening socket");

     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)
              error("ERROR on binding");
     listen(sockfd,5);
     client_thread( (void*) &portno);
     printf("test done\n");
     return 0;
}
// CHECK: Done with lock
