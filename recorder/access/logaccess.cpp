#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

#include "logaccess.h"

namespace tern {

Log::Log() : _fd(-1), _num(0), _buf(NULL) {}

Log::Log(const char* filename) {
  struct stat st;
  long pagesz;

  _fd = open(filename, O_RDONLY);  assert(_fd >= 0);
  fstat(_fd, &st);
  assert(st.st_size % RECORD_SIZE == 0
         && "file size is not multiple of record size!");
  _num = st.st_size / RECORD_SIZE;
  pagesz = getpagesize();
  _mapsz = ((st.st_size+pagesz-1)&~(pagesz-1));
  _buf = (char*)mmap(0, _mapsz, PROT_READ, MAP_SHARED, _fd, 0);

  if(_buf == MAP_FAILED) {
    perror("mmap");
    abort();
  }
}

Log::~Log() {
  if(_buf)
    munmap(_buf, _mapsz);
  if(_fd >= 0)
    close(_fd);
}

Log::rec_iterator Log::rec_begin() {
  return rec_iterator(0, this);
}

Log::rec_iterator Log::rec_end() {
  return rec_iterator(_num, this);
}

Log::reverse_rec_iterator Log::rec_rbegin() {
  return reverse_rec_iterator(rec_end());
}

Log::reverse_rec_iterator Log::rec_rend() {
  return reverse_rec_iterator(rec_begin());
}

} // namespace tern
