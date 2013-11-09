#ifndef __TERN_NON_DET_THREAD_SET_H
#define __TERN_NON_DET_THREAD_SET_H

//#include <iterator>
#include <tr1/unordered_set>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <list>
//#include <string.h>

#define DEBUG_NON_DET_THREAD_SET
#ifdef DEBUG_NON_DET_THREAD_SET
#define ASSERT2(...) assert(__VA_ARGS__)
#else
#define ASSERT2(...)
#endif

namespace tern {
struct non_det_thread_set {
  protected:
    std::list<int> tid_list;
    std::tr1::unordered_map<int, unsigned> tid_to_logical_clock_map;

  public:
    non_det_thread_set() {
      tid_list.clear();
      tid_to_logical_clock_map.clear();
    }
    
    void insert(int tid, unsigned clock) {
      //fprintf(stderr, "non-det-thread-set insert tid %d, clock %u\n", tid, clock);
      ASSERT2(!in(tid));
      tid_list.push_back(tid);
      ASSERT2(tid_to_logical_clock_map.find(tid) == tid_to_logical_clock_map.end());
      tid_to_logical_clock_map[tid] = clock;
    }

    void erase(int tid) {
      bool found = false;
      std::list<int>::iterator itr = tid_list.begin();
      for (; itr != tid_list.end(); itr++) { // TBD: this operation is linear, may be slow.
        if (tid == *itr) {
          found = true;
          break;
        }
      }
      if (found) {
        tid_list.erase(itr);
        tid_to_logical_clock_map.erase(tid);
      } else
        assert(false && "tid must be in the non det set."); // this assertion must be there.
    }

    size_t size() {
      ASSERT2(tid_list.size() == tid_to_logical_clock_map.size());
      return tid_list.size();
    }

    int first_thread() {
      ASSERT2(size()>0);
      return *(tid_list.begin());
    }

    unsigned get_clock(int tid) {
      ASSERT2(tid_to_logical_clock_map.find(tid) != tid_to_logical_clock_map.end());
      return tid_to_logical_clock_map[tid];
    }

    bool in(int tid) {
      if (tid_to_logical_clock_map.find(tid) != tid_to_logical_clock_map.end())
        return true;
      else {
        bool found = false;
        std::list<int>::iterator itr = tid_list.begin();
        for (; itr != tid_list.end(); itr++) {
          if (tid == *itr) {
            found = true;
            break;
          }
        }
        return found;
      }
    }
};
};
#endif
