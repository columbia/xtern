#ifndef __TERN_COMMON_RUNTIME_QUEUE_H
#define __TERN_COMMON_RUNTIME_QUEUE_H

#include <iterator>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_THREAD_NUM 1111

namespace tern {
class run_queue {
  struct runq_elem {
    pthread_spinlock_t spin_lock;
    int tid;
    int status;
    struct runq_elem *prev;
    struct runq_elem *next;
  };
  
  struct runq_elem *head;
  struct runq_elem *tail;
  size_t num_elements;
  struct runq_elem *tid_map[MAX_THREAD_NUM];

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
    clear();
  }

  struct runq_elem *createThreadElem(int tid) {
    //fprintf(stderr, "tid %d is called with runq::createThreadElem\n", tid);
    assert(tid >= 0 && tid < MAX_THREAD_NUM);
    assert(tid_map[tid] == NULL);
    struct runq_elem *elem = new struct runq_elem;
    pthread_spin_init(&(elem->spin_lock), 0);
    elem->tid = tid;
    elem->status = 0; // XXX TO BD DONE.
    elem->prev = NULL;
    elem->next = NULL;
    tid_map[tid] = elem;
    return elem;
  }

  void destroyThreadElem(int tid) {
    print(__FUNCTION__);
    struct runq_elem *elem = tid_map[tid];
    assert(elem);
    tid_map[tid] = NULL;
    pthread_spin_destroy(&(elem->spin_lock));
    delete elem;
  }

  inline iterator begin() {
    return iterator(head);
  }

  inline iterator end() {
    return iterator();
  }

  void clear() {
    print(__FUNCTION__);
    head = tail = NULL;
    num_elements = 0;
    for (int i = 0; i < MAX_THREAD_NUM; i++) // An un-opt version, TBD.
      if (tid_map[i] != NULL) {
        tid_map[i]->prev = tid_map[i]->next = NULL;
      }
  }

  bool empty() {
    print(__FUNCTION__);
    return (size() == 0);
  }
 
  size_t size() {
    return num_elements;
  }

  // Complicated, need more check.
  iterator erase (iterator position) {
    print(__FUNCTION__);
    if (position == end()) {
      return end();
    } else {
      struct runq_elem *ret = position->next;
      struct runq_elem *cur = &position;

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

      num_elements--;
      return iterator(ret);
    }
  }
  
  void push_back(int tid) {
    print(__FUNCTION__);
    //fprintf(stderr, "push back tid %d\n", tid);

    struct runq_elem *elem = tid_map[tid];
    assert(elem);
    if (head == NULL) {
      assert(tail == NULL);
      head = tail = elem;
    } else {
      assert (tail != NULL);
      elem->prev = tail;
      tail->next = elem;
      tail = elem;
    }
    num_elements++;
    print(__FUNCTION__);
  }

  /* This is a thread run queue, all thread ids are fixed, reference is not allowed! */
  int front() {
    print(__FUNCTION__);
    assert(head != NULL);
    return head->tid;
  }

  void push_front(int tid) {
    print(__FUNCTION__);
    struct runq_elem *elem = tid_map[tid];
    assert(elem);
    if (head == NULL) {
      head = tail = elem;
    } else {
      elem->next = head;
      head = elem;
    }
    num_elements++;
  }

  void pop_front() {
    print(__FUNCTION__);
    struct runq_elem *elem = head;
    head = elem->next;
    elem->prev = elem->next = NULL;
    if (head == NULL) /** If head is empty, then the tail must also be empty. **/
      tail = NULL;
    num_elements--;
  }

  void print(const char *tag) {
    return;
    int i = 0;
    fprintf(stderr, "\n\n OP: %s: q size %u\n", tag, (unsigned)size());
    for (run_queue::iterator itr = begin(); itr != end(); ++itr) {
      if (i > MAX_THREAD_NUM)
        assert(false);
      fprintf(stderr, "q[%d] = %d, status = %d\n", i, *itr, itr->status);
      i++;
    }
  }

};
}
#endif

