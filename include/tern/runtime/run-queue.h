#ifndef __TERN_COMMON_RUNTIME_QUEUE_H
#define __TERN_COMMON_RUNTIME_QUEUE_H

#include <iterator>
#include <tr1/unordered_set>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_THREAD_NUM 1111
#define DEBUG_RUN_QUEUE // "defined" means enable the debug check; "undef" means disable it (faster).

namespace tern {
class run_queue {
public:
   enum THD_STATUS {
    RUNNABLE,     /** The thread can do any regular pthreads sync operation. **/
    RUNNING,      /** The thread has got a turn and it should call RRScheduler::block() (getTurn() and then next()),
                              in order to make the turn being passed smoothly. This GOT_TURN is semantically the "token" of the run queue. **/
    NWK_STOP      /** The thread is stopped (blocked) on a network operation, and no other thread can pass turn to it. **/
  };
  
  struct runq_elem {
  public:
    pthread_spinlock_t spin_lock;
    int tid;
    THD_STATUS status;
    struct runq_elem *prev;
    struct runq_elem *next;

    runq_elem(int tid) {
      pthread_spin_init(&spin_lock, 0);
      this->tid = tid;
      status = RUNNABLE;
      prev = next = NULL;
    }
  };

private:
  /** Key members of the run queue. We mainly optimize it for read/write of head/tail. **/
  struct runq_elem *head;
  struct runq_elem *tail;
  size_t num_elements;
  struct runq_elem *tid_map[MAX_THREAD_NUM];

  /** This one is useful only when DEBUG_RUN_QUEUE is defined. **/
  std::tr1::unordered_set<void *> elements;

public:
  class iterator : public std::iterator<std::forward_iterator_tag, int> {
    struct runq_elem *m_rep;
  public:
    friend class run_queue;

    inline iterator(struct runq_elem *x=0):m_rep(x){}
      
    inline iterator(const iterator &x):m_rep(x.m_rep) {}
      
    inline iterator& operator=(const iterator& x) { 
      m_rep=x.m_rep;
      return *this; 
    }

    inline iterator& operator++() { 
      m_rep = m_rep->next;
      return *this; 
    }

    inline iterator operator++(int) { 
      iterator tmp(*this);
      m_rep = m_rep->next;
      return tmp; 
    }

    inline reference operator*() const {
      return m_rep->tid;
    }

    inline struct runq_elem *operator&() const {
      return m_rep;
    }

    // This has compilation problem, so switch to the version below.
    // inline pionter operator->() { 
    inline struct runq_elem *operator->() {
      return m_rep;
    }

    inline bool operator==(const iterator& x) const {
      return m_rep == x.m_rep; 
    }	

    inline bool operator!=(const iterator& x) const {
      return m_rep != x.m_rep; 
    }
  };

  run_queue() {
    memset(tid_map, 0, sizeof(struct runq_elem *)*MAX_THREAD_NUM);
    deep_clear();
  }

  /** Each thread get its own thread element. This is a per-thread array so it is thread-safe. **/
  inline struct runq_elem *get_my_elem(int my_tid) {
    struct runq_elem *elem = tid_map[my_tid];
    assert(elem && my_tid == elem->tid); /** Make sure each thread can only get its own element. **/
    return elem;
  }
  
  inline struct runq_elem *createThreadElem(int tid) {
    //fprintf(stderr, "tid %d is called with runq::createThreadElem\n", tid);
    assert(tid >= 0 && tid < MAX_THREAD_NUM);
    assert(tid_map[tid] == NULL);
    struct runq_elem *elem = new runq_elem(tid);
    tid_map[tid] = elem;
    return elem;
  }

  inline void destroyThreadElem(int tid) {
    print(__FUNCTION__);
    struct runq_elem *elem = tid_map[tid];
    assert(elem);
    tid_map[tid] = NULL;
    pthread_spin_destroy(&(elem->spin_lock));
    delete elem;
  }

  inline void dbg_assert_elem_in(struct runq_elem *elem) {
#ifdef DEBUG_RUN_QUEUE
      assert(elements.find((void *)elem) != elements.end());
#endif
  }

  inline void dbg_assert_elem_not_in(struct runq_elem *elem) {
#ifdef DEBUG_RUN_QUEUE
      assert(elements.find((void *)elem) == elements.end());
#endif
  }

  inline void dbg_insert_elem(struct runq_elem *elem) {
#ifdef DEBUG_RUN_QUEUE
      elements.insert((void *)elem);
#endif
  }

  inline void dbg_erase_elem(struct runq_elem *elem) {
#ifdef DEBUG_RUN_QUEUE
      elements.erase((void *)elem);
#endif
  }

  inline void dbg_clear_all_elems() {
#ifdef DEBUG_RUN_QUEUE
      elements.clear();
#endif
  }

  void dbg_assert_elem_size(size_t sz) {
#ifdef DEBUG_RUN_QUEUE
      assert(elements.size() == sz);
#endif
  }
  
  inline iterator begin() {
    return iterator(head);
  }

  inline iterator end() {
    return iterator();
  }

  /** This is a "deep" clear. It not only clears the list, but also the tid_map.
  This function should only be called when handling fork() and a deep clean is requried. **/
  inline void deep_clear() {
    print(__FUNCTION__);
    head = tail = NULL;
    num_elements = 0;
    dbg_clear_all_elems();
    for (int i = 0; i < MAX_THREAD_NUM; i++) {// An un-opt version, TBD.
      if (tid_map[i] != NULL) {
        int tid = tid_map[i]->tid;
        tid_map[i]->prev = tid_map[i]->next = NULL;
        destroyThreadElem(tid); // Deep clear.
      }
    }
  }

  inline bool empty() {
    print(__FUNCTION__);
    return (size() == 0);
  }
 
  inline size_t size() {
    dbg_assert_elem_size(num_elements);
    return num_elements;
  }

  // Complicated, need more check.
  inline iterator erase (iterator position) {
    print(__FUNCTION__);
    if (position == end()) {
      return end();
    } else {
      struct runq_elem *ret = position->next;
      struct runq_elem *cur = &position;
      dbg_assert_elem_in(cur);

      // Connect the "new" prev and next.
      if (position->prev != NULL)
        position->prev->next = position->next;
      if (position->next != NULL)
        position->next->prev = position->prev;

      // Process head and tail.
      if (iterator(head) == position)
        head = position->next;
      if (iterator(tail) == position)
        tail = position->prev;

      // Clear the position's prev and next.
      cur->prev = cur->next = NULL;

      dbg_erase_elem(cur);
      num_elements--;
      return iterator(ret);
    }
  }
  
  inline void push_back(int tid) {
    print(__FUNCTION__);
    //fprintf(stderr, "push back tid %d\n", tid);

    struct runq_elem *elem = tid_map[tid];
    assert(elem);
    dbg_assert_elem_not_in(elem);
    if (head == NULL) {
      assert(tail == NULL);
      head = tail = elem;
    } else {
      assert (tail != NULL);
      elem->prev = tail;
      tail->next = elem;
      tail = elem;
    }
    dbg_insert_elem(elem);
    num_elements++;
    print(__FUNCTION__);
  }

  /* This is a thread run queue, all thread ids are fixed, reference is not allowed! */
  inline int front() {
    print(__FUNCTION__);
    assert(head != NULL);
    dbg_assert_elem_in(head);
    return head->tid;
  }

  inline struct runq_elem *frontElem() {
    print(__FUNCTION__);
    assert(head != NULL);
    dbg_assert_elem_in(head);
    return head;
  }

  inline void push_front(int tid) {
    print(__FUNCTION__);
    struct runq_elem *elem = tid_map[tid];
    assert(elem);
    dbg_assert_elem_not_in(elem);
    if (head == NULL) {
      head = tail = elem;
    } else {
      elem->next = head;
      head = elem;
    }
    dbg_insert_elem(elem);
    num_elements++;
  }

  inline void pop_front() {
    print(__FUNCTION__);
    struct runq_elem *elem = head;
    dbg_assert_elem_in(elem);
    head = elem->next;
    elem->prev = elem->next = NULL;
    if (head == NULL) /** If head is empty, then the tail must also be empty. **/
      tail = NULL;
    dbg_erase_elem(elem);
    num_elements--;
  }

  inline void print(const char *tag) {
    return;
#ifdef DEBUG_RUN_QUEUE
      int i = 0;
      fprintf(stderr, "\n\n OP: %s: q size %u\n", tag, (unsigned)size());
      for (run_queue::iterator itr = begin(); itr != end(); ++itr) {
        if (i > MAX_THREAD_NUM)
          assert(false);
        fprintf(stderr, "q[%d] = %d, status = %d\n", i, *itr, itr->status);
        i++;
      }
#endif
    }
};
}
#endif

