/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include <stack>
#include <iterator>
#include <tr1/unordered_map>
#include <sys/types.h>
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/BasicBlock.h"
#include "llvm/Support/CallSite.h"
#include "recorder/runtime/logdefs.h"
#include "common/id-manager/IDManager.h"


namespace tern {

struct RawLog {

  // log record iterators
  struct iterator
    : public std::iterator<std::random_access_iterator_tag,
                           InsidRec,ptrdiff_t> {

    typedef iterator self_type;
    typedef std::iterator<std::random_access_iterator_tag,
                          InsidRec, ptrdiff_t> super;
    typedef super::value_type value_type;
    typedef super::difference_type difference_type;
    typedef super::pointer pointer;
    typedef super::reference reference;

    unsigned getIndex() const {
      return index;
    }

    RawLog    *log;
    unsigned index;

    iterator(): log(NULL), index(0) {}
    iterator(RawLog *l, int i): log(l), index(i) {}

    operator pointer() const {
      return (pointer)(log->_buf+index*RECORD_SIZE);
    }
    reference operator*() const {
      assert(index < log->_num);
      return *(pointer)(log->_buf+index*RECORD_SIZE);
    }
    pointer operator->() const {
      return &operator*();
    }
    bool operator==(const RawLog::iterator &rhs) const {
      assert(log == rhs.log && "comparing iterators for different logs!");
      return index == rhs.index;
    }
    bool operator!=(const RawLog::iterator &rhs) const {
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
  }; // struct iterator

  // reverse log record iterators
  typedef std::reverse_iterator<iterator> reverse_iterator;
  RawLog();
  RawLog(const char* file);
  ~RawLog();

  iterator begin();
  iterator end();
  reverse_iterator rbegin();
  reverse_iterator rend();

  int _fd;
  unsigned _num;
  long _mapsz;
  char *_buf;
};


struct InstLog {

  struct ExecutedInstID {
    unsigned inst      : 31;  // either inst ID or index into raw log
    unsigned isLogged  :  1;  // this flag determines so
  };

  typedef RawLog::iterator            raw_iterator;
  typedef RawLog::reverse_iterator    reverse_raw_iterator;

  enum { DefaultInstNum = 1024*1024*1024 };

  raw_iterator raw_begin() { return rawLog->begin(); }
  raw_iterator raw_end()   { return rawLog->end();   }
  reverse_raw_iterator raw_rbegin() { return rawLog->rbegin(); }
  reverse_raw_iterator raw_rend()   { return rawLog->rend();   }

  void append(const RawLog::iterator& ri);
  void append(llvm::Instruction *I);

  InstLog(RawLog* log): rawLog(log) {
    instLog.reserve(DefaultInstNum);
  }

//protected:

  typedef std::vector<ExecutedInstID> InstVec;

  RawLog       *rawLog;
  InstVec      instLog;

public:

  // shared across all logs
  static void setIDManager(llvm::IDManager *IDM);
  static bool funcCallLogged(llvm::Function *F);
  static void readFuncMap(const char* file);

  static llvm::IDManager *IDM;

  typedef std::tr1::unordered_map<llvm::Function*, unsigned> func_map;
  func_map       funcsEscape;
  func_map       funcsCallLogged;
};


struct InstLogBuilder {

  InstLog *create(RawLog *log, IDManager *IDM);

protected:
  void getInstBetween();
  void getInstPrefix();
  void getInstSuffix();

  void incFromBr(llvm::BranchInst *I);
  void incFromReturn(llvm::ReturnInst *I);
  void incFromCall(const llvm::CallSite &cs);

  RawLog::iterator                  cur_ri, nxt_ri;
  llvm::BasicBlock::iterator        cur_ii, nxt_ii;
  std::stack<llvm::BasicBlock::iterator> callStack;

  IDManager   *IDM;
  InstLog     *instLog;
};


llvm::raw_ostream &PrintRecord(llvm::raw_ostream &o, const InsidRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const InsidRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const LoadRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const StoreRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const CallRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const ExtraArgsRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const ReturnRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const SyncRec &rec);

} // namespace tern

#endif
