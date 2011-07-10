/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

#include "logaccess.h"

using namespace llvm;

namespace tern {

Log::iterator::self_type &Log::iterator::operator--() { // predecrement
  BasicBlock *BB = ii->getParent();
  if(ii != BB->begin()) {
    -- ii;

    // if loggable
    //   -- ri
    //   if return, move to call

    return *this;
  }

  -- ri; // if return, find call

  ii = NULL; // get instruction from ri->insid

  return *this;
}

Log::iterator::self_type &Log::iterator::operator++() { // preincrement
  BasicBlock *BB = ii->getParent();
  ++ ii;
  if(ii != BB->end())
    return *this;
  ++ ri; // if call, find first
  ii = NULL; // get instruction from ri->insid
  return *this;
}

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
  return rec_iterator(this, 0);
}

Log::rec_iterator Log::rec_end() {
  return rec_iterator(this, _num);
}

Log::reverse_rec_iterator Log::rec_rbegin() {
  return reverse_rec_iterator(rec_end());
}

Log::reverse_rec_iterator Log::rec_rend() {
  return reverse_rec_iterator(rec_begin());
}

Log::iterator Log::begin() {
  rec_iterator rbegin = rec_begin();
  if(rbegin != rec_end()) {
    unsigned insid = rbegin->insid;
    Instruction *ins = NULL; // find instruction based on insid
    BasicBlock::iterator ii = ins;
    return iterator(rbegin, ii);
  }
  return iterator(rbegin, BasicBlock::iterator());
}

Log::iterator Log::end() {
  rec_iterator rend = rec_end();

  if(rend != rec_begin()) {
    rec_iterator rlast = rend - 1;
    unsigned insid = rlast->insid;
    Instruction *ins = NULL; // find instruction based on insid
    BasicBlock *BB = ins->getParent();
    return iterator(rend, BB->end());
  }
  return iterator(rend, 0); // FIXME
}

Log::reverse_iterator Log::rbegin() {
  return reverse_iterator(end());
}

Log::reverse_iterator Log::rend() {
  return reverse_iterator(begin());
}

} // namespace tern
