/* Definitions of event functions for intra-thread slicer. */


//FILE * fopen ( const char * filename, const char * mode );
DEF(fopen,          OpenClose,           FILE *,           const char * filename, const char * mode)

//int fclose ( FILE * stream );
DEF(fclose,          OpenClose,           int,                 FILE * stream)

//int fflush ( FILE * stream );
//DEF(fflush,          FileOp,           int,                 FILE * stream)

//char * fgets ( char * str, int num, FILE * stream );
//DEF(fgets,           FileOp,            char *,           char * str, int num, FILE * stream)

//void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function);
//DEF(__assert_fail,          Assert,         void,                   const char * assertion, const char * file, unsigned int line, const char * function)

//DEF(pthread_mutex_init,     Lock, int, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)

//DEF(pthread_mutex_lock,     Lock, int, pthread_mutex_t *mutex)

//DEF(pthread_mutex_unlock,   Lock, int, pthread_mutex_t *mutex)

//DEF(pthread_mutex_trylock,  Lock, int, pthread_mutex_t *mutex)

//DEF(pthread_mutex_timedlock,Lock, int, pthread_mutex_t *mutex, const struct timespec *abstime)

//DEF(pthread_mutex_destroy,  Lock, int, pthread_mutex_t *mutex)


