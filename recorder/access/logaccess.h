/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include <sys/types.h>
#include <iterator>
#include "llvm/Instruction.h"
#include "llvm/BasicBlock.h"
#include "recorder/runtime/logdefs.h"

namespace tern {

struct Log {

  // log record iterators
  struct rec_iterator
    : public std::iterator<std::random_access_iterator_tag,
                           InsidRec,ptrdiff_t> {

    typedef rec_iterator self_type;
    typedef std::iterator<std::random_access_iterator_tag,
                          InsidRec, ptrdiff_t> super;
    typedef super::value_type value_type;
    typedef super::difference_type difference_type;
    typedef super::pointer pointer;
    typedef super::reference reference;

    Log *log;
    int64_t index;

    rec_iterator(): log(NULL), index(0) {}
    rec_iterator(Log *l, int i): log(l), index(i) {}

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
      self_type tmp = *this;
      -- *this;
      return tmp;
    }
    self_type operator++(int) { // post increment
      self_type tmp = *this;
      ++ *this;
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

  // reverse log record iterators
  typedef std::reverse_iterator<rec_iterator> reverse_rec_iterator;
  // instruction stream iterators
  struct iterator
    : public std::iterator<std::bidirectional_iterator_tag,
                           llvm::Instruction,ptrdiff_t> {
    typedef iterator self_type;
    typedef std::iterator<std::bidirectional_iterator_tag,
                          llvm::Instruction,ptrdiff_t> super;
    typedef super::value_type value_type;
    typedef super::difference_type difference_type;
    typedef super::pointer pointer;
    typedef super::reference reference;

    rec_iterator               ri;
    llvm::BasicBlock::iterator ii;

    iterator() {}
    iterator(rec_iterator ritor, llvm::BasicBlock::iterator itor)
      : ri(ritor), ii(itor) {}

    operator pointer() const {
      return (pointer) ii;
    }
    reference operator*() const {
      return *(pointer)(ii);
    }
    pointer operator->() const {
      return &operator*();
    }
    bool operator==(const Log::iterator &rhs) const {
      return ri == rhs.ri && ii == rhs.ii;
    }
    bool operator!=(const Log::iterator &rhs) const {
      return ri != rhs.ri || ii != rhs.ii;
    }
    self_type &operator--();
    self_type &operator++();
    self_type operator--(int) { // postdecrement
      self_type tmp = *this;
      -- *this;
      return tmp;
    }
    self_type operator++(int) { // postincrement
      self_type tmp = *this;
      ++ *this;
      return tmp;
    }

    void handleCall();
    void handleReturn();
    void handleBBend();
    void handleBBbegin();
  };

  typedef std::reverse_iterator<iterator> reverse_iterator;

  Log();
  Log(const char* file);
  ~Log();

  rec_iterator rec_begin();
  rec_iterator rec_end();
  reverse_rec_iterator rec_rbegin();
  reverse_rec_iterator rec_rend();

  iterator begin();
  iterator end();
  reverse_iterator rbegin();
  reverse_iterator rend();

  int _fd;
  int64_t _num;
  long _mapsz;
  char *_buf;
};

struct LogManager {
};

} // namespace tern

#endif
