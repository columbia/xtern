#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h> 
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "assert.h"

#define NCLIENT 4

struct mysock
{
  mysock(int sock, const struct sockaddr_in *addr)
  {
    sockfd = sock;
    bcopy((char *)addr, (char *)&sockaddr, sizeof(struct sockaddr_in));
  }

  mysock()
  {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &sockaddr, sizeof(sockaddr));
  }

  ~mysock()
  {
    close(); 
  }

  struct sockaddr_in sockaddr;

  bool bind(int port)
  {
    struct sockaddr_in &serv_addr = sockaddr;

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (::bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) 
      return 0;
    
    return 1;
  }

  bool listen(int port)
  {
    if (!bind(port)) return 0;
    ::listen(sockfd, 5);
    return 1;
  }

  mysock *accept()
  {
    int clisock;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    fprintf(stderr, "size = %d, sa_family = %d", 
      (int) sizeof(struct sockaddr_in), (int) cli_addr.sin_family);
    clisock = ::accept(sockfd, 
                (struct sockaddr *) &cli_addr, 
                &clilen);

    fprintf(stderr, "accept: port = %d\n", (int) ntohs(cli_addr.sin_port));
    return new mysock(clisock, &cli_addr);
  }

  bool connect(const char *sockname, int port)
  {
    struct hostent *server;
    struct sockaddr_in &serv_addr = sockaddr;

    server = gethostbyname(sockname);
    if (server == NULL) {
      fprintf(stderr,"ERROR, no such host\n");
      return 0;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);

    serv_addr.sin_port = htons(port);

    if (::connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
      return 0;

    socklen_t len = sizeof(serv_addr);
    getsockname(sockfd, (struct sockaddr *)&serv_addr, &len);
    fprintf(stderr, "port = %d->%d\n", port, (int) ntohs(serv_addr.sin_port));
    
    return 1;
  }

  int send(const void *buffer, int len)
  {
    return (int)::write(sockfd, buffer, len);
  }

  int recv(void *buffer, int len)
  {
    return (int)::read(sockfd, buffer, len);
  }

  int close() {
    return ::close(sockfd);
  }

  int sockfd;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

void *worker_thread(void *param)
{
  uint64_t *args = (uint64_t*)param;
  mysock *sock = (mysock*)args[0];
  pthread_mutex_t *x = (pthread_mutex_t*) args[1];
  int *pcount = (int*) args[2];
  int tid = (int) args[3];

  int xtid = -1;
  sock->recv(&xtid, sizeof(xtid));
  fprintf(stderr, "thread %d gets message %d from server.\n", tid, xtid);

  pthread_mutex_lock(x);
  //*pcount = *pcount + 1;
  (*pcount)++;    // don't use this, it's dangerous in multithreaded program.
  //++*pcount;
  fprintf(stdout, "thread %d obtains mutex, count = %d, pcount = %p.\n", tid, *pcount, pcount);
  pthread_mutex_unlock(x);
}

void *client_thread(void *param)
{
    uint64_t *args = (uint64_t*)param;

    char buffer[256];

    int portno = (int) (uint64_t) param;
    fprintf(stderr, "client thread starting\n");

    for (int i = 0; i < NCLIENT; ++i)
    {
      mysock *sock = new mysock();
      
      fprintf(stderr, "client sets up %dth connection, port = %d\n", i, portno);
      sock->connect("127.0.0.1", portno);
      fprintf(stderr, "client sending message to %dth connection\n", i);
      sock->send(&i, sizeof(i));
    }

//    sockfd = socket(AF_INET, SOCK_STREAM, 0);
//    if (sockfd < 0) 
//        error("ERROR opening socket");
//    server = gethostbyname("127.0.0.1");
//    if (server == NULL) {
//        fprintf(stderr,"ERROR, no such host\n");
//        exit(0);
//    }
//    bzero((char *) &serv_addr, sizeof(serv_addr));
//    serv_addr.sin_family = AF_INET;
//    bcopy((char *)server->h_addr, 
//         (char *)&serv_addr.sin_addr.s_addr,
//         server->h_length);
//    serv_addr.sin_port = htons(portno);
//    sleep(1);
//    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
//        error("ERROR connecting");
//    printf("Please enter the message: ");
//    bzero(buffer,256);
    //fgets(buffer,255,stdin);

//    n = write(sockfd,buffer,strlen(buffer));
//    if (n < 0) 
//         error("ERROR writing to socket");
//    bzero(buffer,256);
//    n = read(sockfd,buffer,255);
//    if (n < 0) 
//         error("ERROR reading from socket");
//    close(sockfd);
    return 0;
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     int fd;
    
     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }

//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0) 
//        error("ERROR opening socket");
//     bzero((char *) &serv_addr, sizeof(serv_addr));
//     portno = atoi(argv[1]);
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_addr.s_addr = INADDR_ANY;
//     serv_addr.sin_port = htons(portno);
//     if (bind(sockfd, (struct sockaddr *) &serv_addr,
//              sizeof(serv_addr)) < 0) 
//              error("ERROR on binding");
//     listen(sockfd,5);

     pthread_mutex_t x;
     pthread_mutex_init(&x, NULL);
     pthread_t th[NCLIENT];
     mysock *sock = new mysock();
     time_t tt;
     tt = time(&tt);
     srand(tt);
     portno = atoi(argv[1]) + rand() % 500;
     sock->listen(portno);

     pthread_t clientth; 
     fprintf(stderr, "creating client thread\n");
     pthread_create(&clientth, NULL, client_thread, (void*)(uint64_t) portno);

     int count = 0;
     for (int i = 0; i < NCLIENT; ++i)
     {
       fprintf(stderr, "accepting for %d worker thread\n", i);
       mysock *conn = sock->accept();

       uint64_t *args = new uint64_t[4];
       args[0] = (uint64_t) conn;
       args[1] = (uint64_t) &x;
       args[2] = (uint64_t) &count;
       args[3] = i;

       fprintf(stderr, "creating %d worker thread\n", i);

       pthread_create(&th[i], NULL, worker_thread, (void*) args);
     }

//     clilen = sizeof(cli_addr);
//     newsockfd = accept(sockfd, 
//                 (struct sockaddr *) &cli_addr, 
//                 &clilen);
//     if (newsockfd < 0) 
//          error("ERROR on accept");
//     bzero(buffer,256);
//     n = read(newsockfd,buffer,255);
//     if (n < 0) error("ERROR reading from socket");

//     n = write(newsockfd,"I got your message",18);
//     if (n < 0) error("ERROR writing to socket");
//     close(newsockfd);
//     close(sockfd);
     for (int i = 0; i < NCLIENT; ++i)
       pthread_join(th[i], NULL);
     pthread_join(clientth, NULL);

     fprintf(stderr, "count = %d\n", count);

     return 0; 
}

