/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include <map>
#include <list>
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
#include "tern/logdefs.h"

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
  unsigned numRecords() { return _num; }

  int      _fd;
  unsigned _num;
  long     _mapsz;
  char*    _buf;
};

// TODO: should move the code below to the analysis/ folder, and separate
// them into different files

struct TurnRange {
  bool happensBefore(const TurnRange &tr) const {
    return endTurn <= tr.beginTurn;
  }
  bool concurrent(const TurnRange &tr) const {
    return !happensBefore(tr)
      && !tr.happensBefore(*this);
  }

  unsigned beginTurn, endTurn;
};

struct InstLog;
struct InstTrunk: public TurnRange {
  InstLog *instLog;
  unsigned beginIndex, endIndex; // indexes into InstLog

  InstTrunk() {
    instLog = NULL;
    beginTurn = endTurn = (unsigned)-1;
    beginIndex = endIndex = (unsigned)-1;
  }
  InstTrunk(InstLog *log, unsigned turn, unsigned index) {
    instLog = log;
    beginTurn = turn;
    beginIndex = index;
    endTurn = (unsigned)-1;
    endIndex = (unsigned)-1;
  }
};

// logged instructions
struct LInst {
  llvm::Instruction *inst;
  InsidRec          *rec;

  LInst(llvm::Instruction *I, InsidRec *r)
    : inst(I), rec(r) {}

  unsigned  getRecType() const { return rec->type; }
};

struct LMemInst: public LInst {
  char*     getAddr() const;
  unsigned  getDataSize() const;
  uint64_t  getData() const;
  uint8_t   getDataByte(unsigned bytenr) const;
};

struct LLoadInst : public LMemInst {};
struct LStoreInst: public LMemInst {};

struct LCallInst: public LInst {
  llvm::Function* getCalledFunction();
  unsigned numArgs(void) const;
  unsigned getArgSize(unsigned argnr) const;
  uint64_t getArgData(unsigned argnr) const;
  uint8_t  getArgDataByte(unsigned argnr, unsigned bytenr) const;
  unsigned getRetDataSize() const;
  uint64_t getRetData() const;
  uint8_t  getRetDataByte(unsigned bytenr) const;
};

template<typename To>
To& LInstCast(LInst& LI) {
  // TODO: assert rec->type match type To
  return (To&)(LI);
}
template<typename To>
const To& LInstCast(const LInst& LI) {
  // TODO: assert rec->type match type To
  return (To&)(LI);
}
template<typename To>
To* LInstCast(LInst* LI) {
  // TODO: assert rec->type match type To
  return (To*)(LI);
}

struct InstLog {

public:

  typedef RawLog::iterator            raw_iterator;
  typedef RawLog::reverse_iterator    reverse_raw_iterator;
  typedef std::vector<InstTrunk>      TrunkVec;
  typedef TrunkVec::iterator          trunk_iterator;
  typedef TrunkVec::reverse_iterator  reverse_trunk_iterator;

  trunk_iterator trunk_begin()      { return trunks.begin();  }
  trunk_iterator trunk_end()        { return trunks.end();    }
  reverse_trunk_iterator trunk_rbegin() { return trunks.rbegin();  }
  reverse_trunk_iterator trunk_rend()   { return trunks.rend();    }

  raw_iterator raw_begin()          { return rawLog->begin();  }
  raw_iterator raw_end()            { return rawLog->end();    }
  reverse_raw_iterator raw_rbegin() { return rawLog->rbegin(); }
  reverse_raw_iterator raw_rend()   { return rawLog->rend();   }

  llvm::raw_ostream &printDInst(llvm::raw_ostream &o,
                                unsigned idx, bool details=false);
  llvm::raw_ostream &printRawRec(llvm::raw_ostream &o,
                                 const InstLog::raw_iterator&);

  unsigned numTrunks() { return trunks.size(); }
  unsigned getThreadEndTurn() { return trunks.back().endTurn; }
  unsigned getThreadBeginTurn() { return trunks.front().beginTurn; }

  unsigned numDInsts() { return instLog.size(); }
  bool isDInstLogged(unsigned idx);
  /// only for not logged instructions
  llvm::Instruction *getStaticInst(unsigned idx);
  /// only for logged instructions or tern_thread_begin and
  /// tern_thread_end records
  LInst getLInst(unsigned idx);

  // not-logged call/invoke instructions
  llvm::Function *getCalledFunction(unsigned callIdx);
  unsigned getReturnFromCall (unsigned callIdx);
  unsigned getCallFromReturn (unsigned retIdx);

  // branch, indirect branch, switch, or unwind instructions (not logged)
  llvm::BasicBlock *getJumpTarget(unsigned idx);
  llvm::BasicBlock *getJumpSource(unsigned idx);

  InstLog(RawLog* log);
  ~InstLog() { delete rawLog; }

protected:
  // TODO: do we have enough memory to hold a bigger struct per executed
  // instruction?  If so, we can have a whole suite of DynInst classes.
  // or perhaps this should be two pointers, Instruction* and InsidRec*
  //
  struct DInst {
    unsigned inst     : 31;
    unsigned isLogged : 1;
  };

  typedef std::vector<DInst>          InstVec;
  typedef std::tr1::unordered_map<unsigned, unsigned> IndexMap;

  /// returns index of appended instruction
  unsigned append(const RawLog::iterator& ri);
  unsigned append(llvm::Instruction *I);
  void matchCallReturn(unsigned callIdx, unsigned retIdx);

  void verify();

  friend class InstLogBuilder;

protected:

  RawLog*      rawLog;
  InstVec      instLog;
  IndexMap     callRetMap;  /// call instruction idx -> return instruction idx
  IndexMap     retCallMap;  /// return instruction idx -> call instruction idx

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
  void nextInstFromLoggedInst();

  void dump();

  InstLog                          *instLog;
  unsigned                          cur_idx, call_idx;
  RawLog::iterator                  cur_ri, nxt_ri;
  llvm::BasicBlock::iterator        cur_ii, nxt_ii;
  std::stack<std::pair<llvm::BasicBlock::iterator, unsigned> > callStack;
};


struct RacyEdgeHalf {
  const InstTrunk  *trunk;
  unsigned         idx; // index into InstLog
  RacyEdgeHalf     *otherHalf;

  RacyEdgeHalf(): trunk(NULL), idx((unsigned)-1), otherHalf(NULL) {}
  RacyEdgeHalf(const InstTrunk *tr, unsigned i, RacyEdgeHalf *other)
    : trunk(tr), idx(i), otherHalf(other) { }
};

struct RacyEdge {
  RacyEdgeHalf up, down;

  RacyEdge() {}
  RacyEdge(const InstTrunk *t1, unsigned i1, const InstTrunk *t2, unsigned i2)
    : up(t1, i1, &down), down(t2, i2, &up) { }
};

struct ProgInstLog {

  typedef std::map<unsigned, InstTrunk*> TrunkMap;

  void create(int nthread);

  TrunkMap                trunks;  // indexed by beginTurn
  std::vector<InstLog*>   threadLogs;
  std::list<RacyEdge>     racyEdges;
};


llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const InstTrunk& tr);

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const InsidRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const LoadRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const StoreRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const CallRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const ExtraArgsRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const ReturnRec &rec);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const SyncRec &rec);

llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const RacyEdgeHalf &reh);
llvm::raw_ostream &operator<< (llvm::raw_ostream &o, const RacyEdge &re);

} // namespace tern

#endif
