/* Definitions of summaries of external functions.
    Please be careful that the function names here must be the names after linking in uclibc,
    for example, fwrite() in original bc becomes fwrite_unlocked() after linking in uclibc. */

//void * memcpy ( void * destination, const void * source, size_t num );
DEF(memcpy, ExtStore, ExtLoad, ExtReadVal)

//void * memset ( void * ptr, int value, size_t num );
DEF(memset, ExtStore, ExtReadVal, ExtReadVal)

//int strcmp ( const char * str1, const char * str2 );
DEF(strcmp, ExtLoad, ExtLoad)

//char * strcpy ( char * destination, const char * source );
DEF(strcpy, ExtStore, ExtLoad)

