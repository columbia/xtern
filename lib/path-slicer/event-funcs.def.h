/* Definitions of event functions for intra-thread slicer. */


DEF(fopen,          FileOp,           FILE *,           const char * filename, const char * mode)
DEF(fclose,          FileOp,           int,                 FILE * stream)
DEF(fflush,          FileOp,           int,                 FILE * stream)
DEF(fgets,           FileOp,            char *,           char * str, int num, FILE * stream)

