/* Definitions of event functions for intra-thread slicer. */


//FILE * fopen ( const char * filename, const char * mode );
DEF(fopen,          FileOp,           FILE *,           const char * filename, const char * mode)

//int fclose ( FILE * stream );
DEF(fclose,          FileOp,           int,                 FILE * stream)

//FILE *fopen64(const char *filename, const char *type);
DEF(fopen64, FileOp,                FILE *,                const char *filename, const char *type)

//int fputs ( const char * str, FILE * stream );
DEF(fputs,      FileOp,                int,                        const char * str, FILE * stream )
DEF(fputs_unlocked,      FileOp,            int,                        const char * str, FILE * stream )

//char * fgets ( char * str, int num, FILE * stream );
DEF(fgets,           FileOp,            char *,           char * str, int num, FILE * stream)
DEF(fgets_unlocked,           FileOp,            char *,           char * str, int num, FILE * stream)

//int fprintf ( FILE * stream, const char * format, ... );
DEF(fprintf,        FileOp,              int,               FILE * stream, const char * format, ... )

//size_t fread ( void * ptr, size_t size, size_t count, FILE * stream );
DEF(fread,                    FileOp,         size_t,                void * ptr, size_t size, size_t count, FILE * stream )
DEF(fread_unlocked,                    FileOp,         size_t,                void * ptr, size_t size, size_t count, FILE * stream )


//size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );
DEF(fwrite,               FileOp,               size_t,                  const void * ptr, size_t size, size_t count, FILE * stream )
DEF(fwrite_unlocked,               FileOp,               size_t,                  const void * ptr, size_t size, size_t count, FILE * stream )

//int fflush ( FILE * stream );
DEF(fflush,             FileOp,         int, FILE * stream )
DEF(fflush_unlocked,             FileOp,         int, FILE * stream )

//int fsync(int fildes);
DEF(fsync,      FileOp,            int,                        int fildes)

//int fileno(FILE *stream);
DEF(fileno,         FileOp,             int,              FILE *stream)
DEF(fileno_unlocked,         FileOp,             int,              FILE *stream)

//int rename(const char *old, const char *new);
DEF(rename,             FileOp,                int,          const char *old, const char *new)




//void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function);
DEF(__assert_fail,          Assert,         void,                   const char * assertion, const char * file, unsigned int line, const char * function)
















//DEF(pthread_mutex_init,     Lock, int, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)

//DEF(pthread_mutex_lock,     Lock, int, pthread_mutex_t *mutex)

//DEF(pthread_mutex_unlock,   Lock, int, pthread_mutex_t *mutex)

//DEF(pthread_mutex_trylock,  Lock, int, pthread_mutex_t *mutex)

//DEF(pthread_mutex_timedlock,Lock, int, pthread_mutex_t *mutex, const struct timespec *abstime)

//DEF(pthread_mutex_destroy,  Lock, int, pthread_mutex_t *mutex)


