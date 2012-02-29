/* Definitions of summaries of external functions.
    Please be careful that the function names here must be the names after linking in uclibc,
    for example, fwrite() in original bc becomes fwrite_unlocked() after linking in uclibc. */

//void * memchr (       void * ptr, int value, size_t num );
DEF(memchr, ExtLoad, ExtReadVal, ExtReadVal)

//int memcmp ( const void * ptr1, const void * ptr2, size_t num );
DEF(memcmp, ExtLoad, ExtLoad, ExtReadVal)

//void * memcpy ( void * destination, const void * source, size_t num );
DEF(memcpy, ExtStore, ExtLoad, ExtReadVal)

//void * memmove ( void * destination, const void * source, size_t num );
DEF(memmove, ExtStore, ExtLoad, ExtReadVal)

//void * memset ( void * ptr, int value, size_t num );
DEF(memset, ExtStore, ExtReadVal, ExtReadVal)

//char * strcat ( char * destination, const char * source );
DEF(strcat, ExtStore, ExtLoad)

//char * strchr (       char * str, int character );
DEF(strchr, ExtLoad, ExtReadVal)

//int strcmp ( const char * str1, const char * str2 );
DEF(strcmp, ExtLoad, ExtLoad)

//int strcoll ( const char * str1, const char * str2 );
DEF(strcoll, ExtLoad, ExtLoad)

//char * strcpy ( char * destination, const char * source );
DEF(strcpy, ExtStore, ExtLoad)

//size_t strcspn ( const char * str1, const char * str2 );
DEF(strcspn, ExtLoad, ExtLoad)

//char * strerror ( int errnum );
DEF(strerror, ExtReadVal)

//size_t strlen ( const char * str );
DEF(strlen, ExtLoad)

//char * strncat ( char * destination, char * source, size_t num );
DEF(strncat, ExtStore, ExtLoad, ExtReadVal)

//int strncmp ( const char * str1, const char * str2, size_t num );
DEF(strncmp, ExtLoad, ExtLoad, ExtReadVal)

//char * strncpy ( char * destination, const char * source, size_t num );
DEF(strncpy, ExtStore, ExtLoad, ExtReadVal)

//char * strpbrk (       char * str1, const char * str2 );
DEF(strpbrk, ExtLoad, ExtLoad)

//char * strrchr (       char * str, int character );
DEF(strrchr, ExtLoad, ExtReadVal)

//size_t strspn ( const char * str1, const char * str2 );
DEF(strspn, ExtLoad, ExtLoad)

//char * strstr (       char * str1, const char * str2 );
DEF(strstr, ExtLoad, ExtLoad)

//char * strtok ( char * str, const char * delimiters );
/* Since the first argument "str" is splitted, '\0' must have been assigned to
some locations of "str", so it is a ExtStore. */
DEF(strtok, ExtStore, ExtLoad)

//size_t strxfrm ( char * destination, const char * source, size_t num );
DEF(strxfrm, ExtStore, ExtLoad, ExtReadVal)















