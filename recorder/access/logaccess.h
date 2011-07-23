/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include <map>
#include <stack>
#include <iterator>
#include <tr1/unordered_map>
#include <sys/types.h>
#include "llvm/Module.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/BasicBlock.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "recorder/runtime/logdefs.h"

namespace tern {

struct RawLog {

  // log record iterators
  struct iterator
    : public std::iterator<std::random_access_iterator_tag,
                           InsidRec,ptrdiff_t> {

    typedef iterator self_type;
    typedef std::iterator<std::random_access_iterator_tag,
                          InsidRec, ptrdiff_t> super;
    typedef super::value_type      value_type;
    typedef super::difference_type difference_type;
    typedef super::pointer         pointer;
    typedef super::reference       reference;

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

  int      _fd;
  unsigned _num;
  long     _mapsz;
  char*    _buf;
};

// TODO: should move the code below to the analysis/ folder, and separate
// them into different files

#if 0
struct InstLog;
struct DInstruction {
  int turn;
  InstLog *log;
  InstClass *inst;
  int getOpcode() { return inst->getOpcode(); }
  Instruction *getInstruction() { return inst; }
};

struct RInstruction: public DInstruction {
  InsidRec *rec;
};

struct RLoadInst: public RInstruction {
  void*    getAddr();
  int      getSize();
  uint64_t getData();
};

struct RStoreInst: public RInstruction {
  void*    getAddr();
  int      getSize();
  uint64_t getData();
};

struct RCallInst: public RInstruction {
  llvm::Function* getCalledFunction();
  uint64_t getArg(unsigned i);
  int getArgSize(unsigned i);
  int numArgs(void);
  uint64_t getReturn();
  int getReturnSize();
};
#endif

struct InstLog {

  // TODO: do we have enough memory to hold a bigger struct per executed
  // instruction?  If so, we can have a whole suite of DynInst classes.
  // or perhaps this should be two pointers, Instruction* and InsidRec*
  //
  struct DInst {
    unsigned inst      : 31;  // either inst ID or index into raw log
    unsigned isLogged  :  1;  // this flag determines so
  };

  struct Trunk {
    InstLog *instLog;
    unsigned beginTurn, endTurn;
    unsigned beginIndex, endIndex; // indexes into InstLog

    bool happensBefore(const Trunk &tr) const {
      return endTurn <= tr.beginTurn;
    }
    bool concurrent(const Trunk &tr) const {
      return !happensBefore(tr)
        && !tr.happensBefore(*this);
    }

    Trunk(): instLog(NULL) {}
    Trunk(InstLog *log, unsigned beginT, unsigned beginI)
      : instLog(log), beginTurn(beginT), beginIndex(beginI) {}
  };

  typedef RawLog::iterator            raw_iterator;
  typedef RawLog::reverse_iterator    reverse_raw_iterator;
  typedef std::vector<DInst>          InstVec;
  typedef InstVec::iterator           iterator;
  typedef InstVec::reverse_iterator   reverse_iterator;
  typedef std::vector<Trunk>          TrunkVec;
  typedef TrunkVec::iterator          trunk_iterator;
  typedef TrunkVec::reverse_iterator  reverse_trunk_iterator;

  enum { DefaultInstNum = 1024*1024*1024 };

  raw_iterator raw_begin()          { return rawLog->begin();  }
  raw_iterator raw_end()            { return rawLog->end();    }
  reverse_raw_iterator raw_rbegin() { return rawLog->rbegin(); }
  reverse_raw_iterator raw_rend()   { return rawLog->rend();   }
  iterator begin()                  { return instLog.begin();  }
  iterator end()                    { return instLog.end();    }
  reverse_iterator rbegin()         { return instLog.rbegin(); }
  reverse_iterator rend()           { return instLog.rend();   }
  trunk_iterator trunk_begin()      { return trunks.begin();  }
  trunk_iterator trunk_end()        { return trunks.end();    }
  reverse_trunk_iterator trunk_rbegin() { return trunks.rbegin();  }
  reverse_trunk_iterator trunk_rend()   { return trunks.rend();    }

  void append(const RawLog::iterator& ri);
  void append(llvm::Instruction *I);

  llvm::raw_ostream &printExecutedInst(llvm::raw_ostream &o,
                            DInst id, bool details=false);
  llvm::raw_ostream &printRawRec(llvm::raw_ostream &o,
                            const InstLog::raw_iterator&);

  unsigned numTrunks() { return trunks.size(); }
  unsigned getThreadEndTurn() { return trunks.back().endTurn; }
  unsigned getThreadBeginTurn() { return trunks.front().beginTurn; }

  DInst getDInst(int i) { return instLog[i]; }
  InsidRec *getRawRec(DInst di);
  llvm::Instruction *getInst(DInst di);

  // load/store instructions
  void *getAddr(DInst di);
  int getSize(DInst di); // in bytes
  uint64_t getData(DInst di);

  // call instruction
  llvm::Function *getCalledFunction(DInst di);
  int getReturnSize(DInst di);
  uint64_t getReturnData(DInst di);

  void verify();

  InstLog(RawLog* log): rawLog(log) { instLog.reserve(DefaultInstNum); }
  ~InstLog() { delete rawLog; }

protected:

  RawLog*      rawLog;
  InstVec      instLog;
  TrunkVec     trunks;

public:

  // shared across all logs.  TODO: make this a separate struct
  static unsigned getFuncID(llvm::Function *F);
  static llvm::Function *getFunc(unsigned id);
  static void setFuncMap(const char *file, const llvm::Module& M);

  typedef std::tr1::unordered_map<llvm::Function*, unsigned> func_map;
  typedef std::tr1::unordered_map<unsigned, llvm::Function*> reverse_func_map;

  static func_map         funcsEscape;
  static func_map         funcsCallLogged;
  static reverse_func_map funcsIDMap;
};


struct InstLogBuilder {

  InstLog *create(const char* logfile);

protected:
  InstLog *create(RawLog *log);
  void appendInbetweenInsts(bool takeCurrent=true);
  void appendInsts();
  void appendInstsPrefix();

  int nextInstFromJmp();
  int nextInstFromReturn();
  int nextInstFromCall();

  void dump();

  InstLog                          *instLog;
  RawLog::iterator                  cur_ri, nxt_ri;
  llvm::BasicBlock::iterator        cur_ii, nxt_ii;
  std::stack<llvm::BasicBlock::iterator> callStack;
};


struct ProgInstLog {

  typedef std::map<unsigned, InstLog::Trunk*> TrunkMap;

  struct RaceHalf {
    InstLog::Trunk *trunk;
    int index; // index into InstLog
    RaceHalf  *otherHalf;

    llvm::raw_ostream& operator<<(llvm::raw_ostream&) const;
  };

  struct Race {
    RaceHalf first, second;
    llvm::raw_ostream& operator<<(llvm::raw_ostream&) const;
  };

  void create(int nthread);

  TrunkMap                trunks;  // indexed by beginTurn
  std::vector<InstLog*>   threadLogs;
  std::vector<Race>       races;
};


llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const InstLog::Trunk& tr);

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const InsidRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const LoadRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const StoreRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const CallRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const ExtraArgsRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const ReturnRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const SyncRec &rec);

} // namespace tern

#endif
