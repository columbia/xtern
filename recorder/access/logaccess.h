/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include <sys/types.h>
#include "recorder/runtime/logdefs.h"
#include <iterator>

namespace tern {

struct Log {

  // log record iterators
  struct rec_iterator
    : public std::iterator<std::random_access_iterator_tag,InsidRec,ptrdiff_t> {

    typedef rec_iterator self_type;
    typedef std::iterator<std::bidirectional_iterator_tag,
                          InsidRec, ptrdiff_t> super;
    typedef super::value_type value_type;
    typedef super::difference_type difference_type;
    typedef super::pointer pointer;
    typedef super::reference reference;

    int64_t index;
    Log *log;

    rec_iterator(): index(0), log(NULL) {}
    rec_iterator(int i, Log *l): index(i), log(l) {}

    operator pointer() const {
      return (pointer)(log->_buf+index*RECORD_SIZE);
    }
    reference operator*() const {
      assert(index >= 0 && index < log->_num);
      return *(pointer)(log->_buf+index*RECORD_SIZE);
    }
    pointer operator->() const {
      return &operator*();
    }
    bool operator==(const Log::rec_iterator &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index == rhs.index;
    }
    bool operator!=(const Log::rec_iterator &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index != rhs.index;
    }
    self_type &operator--() { // predecrement
      assert(log);
      assert(index > 0 && "--'d off the beginning of an log!");
      -- index;
      return *this;
    }
    self_type &operator++() { // preincrement
      assert(log);
      assert(index <= log->_num && "++'d off the end of an log!");
      ++ index;
      return *this;
    }
    self_type operator--(int) { // postdecrement
      assert(log);
      assert(index > 0 && "--'d off the beginning of an log!");
      self_type tmp = *this;
      -- index;
      return tmp;
    }
    self_type operator++(int) { // post increment
      assert(log);
      assert(index <= log->_num && "++'d off the end of an log!");
      self_type tmp = *this;
      ++ index;
      return tmp;
    }
    // random access methods
    self_type& operator+=(difference_type n) {
      index += n;
      return *this;
    }
    self_type& operator-=(difference_type n){
      index -= n;
      return *this;
    }
    self_type operator+(difference_type n) const {
      self_type tmp = *this;
      tmp += n;
      return tmp;
    }
    self_type operator-(difference_type n) const {
      self_type tmp = *this;
      tmp -= n;
      return tmp;
    }
    reference  operator[](difference_type n) const {
      self_type tmp = *this;
      tmp += n;
      return *tmp;
    }
    difference_type operator-(self_type &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index - rhs.index;
    }
    bool operator>(self_type &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index > rhs.index;
    }
    bool operator>=(self_type &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index >= rhs.index;
    }
    bool operator<(self_type &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index < rhs.index;
    }
    bool operator<=(self_type &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index <= rhs.index;
    }
  }; // struct rec_iterator

  typedef std::reverse_iterator<rec_iterator> reverse_rec_iterator;

  // instruction stream iterators
  struct iterator;
  struct reverse_iterator;

  Log();
  Log(const char* file);
  ~Log();

  rec_iterator rec_begin();
  rec_iterator rec_end();
  reverse_rec_iterator rec_rbegin();
  reverse_rec_iterator rec_rend();

  int _fd;
  int64_t _num;
  long _mapsz;
  char *_buf;
};

struct LogManager {
};

} // namespace tern

#endif
