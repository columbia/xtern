#include <pthread.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
extern "C" void __gxx_personality_v0(void) {} 

extern "C" void hello_world();

extern "C" void hey()
{
  printf("hello a.out, from .so\n");
}

#define PRINT_DEBUG
extern "C" int pthread_create(pthread_t *thread,const pthread_attr_t *attr,void *(*start_routine)(void*),void *arg){
	typedef int (*orig_func_type)(pthread_t *,const pthread_attr_t *,void *(*)(void*),void *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_create");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(thread, attr, start_routine, arg);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_create is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int pthread_mutex_lock(pthread_mutex_t *mutex){
	typedef int (*orig_func_type)(pthread_mutex_t *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_mutex_lock");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(mutex);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_mutex_lock is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int pthread_mutex_trylock(pthread_mutex_t *mutex){
	typedef int (*orig_func_type)(pthread_mutex_t *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_mutex_trylock");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(mutex);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_mutex_trylock is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int pthread_mutex_unlock(pthread_mutex_t *mutex){
	typedef int (*orig_func_type)(pthread_mutex_t *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_mutex_unlock");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(mutex);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_mutex_unlock is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex){
	typedef int (*orig_func_type)(pthread_cond_t *, pthread_mutex_t *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_cond_wait");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(cond, mutex);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_cond_wait is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime){
	typedef int (*orig_func_type)(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_cond_timedwait");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(cond, mutex, abstime);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_cond_timedwait is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int pthread_join(pthread_t thread, void **retval){
	typedef int (*orig_func_type)(pthread_t, void**);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libpthread.so.0", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "pthread_join");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(thread, retval);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: pthread_join is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int socket(int domain, int type, int protocol){
	typedef int (*orig_func_type)(int, int, int);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

  hello_world();

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "socket");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(domain, type, protocol);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: socket is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" ssize_t send(int sockfd, const void* buf, size_t len, int flags){
	typedef int (*orig_func_type)(int, const void*, size_t, int);
  
	orig_func_type orig_func;

	void * handle;
	ssize_t ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "send");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(sockfd, buf, len, flags);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: send is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" ssize_t recv(int sockfd, void* buf, size_t len, int flags){
	typedef int (*orig_func_type)(int, void*, size_t, int);
  
	orig_func_type orig_func;

	void * handle;
	ssize_t ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "recv");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(sockfd, buf, len, flags);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: recv is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
	typedef int (*orig_func_type)(int , const struct sockaddr *, socklen_t);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "connect");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(sockfd, addr, addrlen);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: connect is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
	typedef int (*orig_func_type)(int , struct sockaddr *, socklen_t *);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "accept");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(sockfd, addr, addrlen);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: accept is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int listen(int sockfd, int backlog){
	typedef int (*orig_func_type)(int, int);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "listen");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(sockfd, backlog);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: listen is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" ssize_t read(int fd, void *buf, size_t count){
	typedef int (*orig_func_type)(int, void*, size_t);
  
	orig_func_type orig_func;

	void * handle;
	ssize_t ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "read");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(fd, buf, count);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: read is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" ssize_t write(int fd, const void *buf, size_t count){
	typedef int (*orig_func_type)(int, const void*, size_t);
  
	orig_func_type orig_func;

	void * handle;
	ssize_t ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "write");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(fd, buf, count);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: write is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int sigwait(const sigset_t *set, int *sig){
	typedef int (*orig_func_type)(const sigset_t *, int*);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "sigwait");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(set, sig);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: sigwait is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

extern "C" int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout){
	typedef int (*orig_func_type)(int, fd_set*, fd_set *, fd_set*, struct timeval*);
  
	orig_func_type orig_func;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		perror("dlopen");
		puts("here dlopen");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "select");

	if(dlerror()) {
		perror("dlsym");
		puts("here dlsym");
		abort();
	}

	dlclose(handle);

	ret = orig_func(nfds, readfds, writefds, exceptfds, timeout);

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: select is hooked.\n", (int) pthread_self());
#endif
	return ret;
}

