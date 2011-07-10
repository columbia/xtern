#include "gtest/gtest.h"
#include "recorder/runtime/log.h"
#include "recorder/access/logaccess.h"

using namespace tern;

struct loadstore {
  unsigned type;
  unsigned insid;
  void *addr;
  uint64_t data;
};

loadstore loadstores[5] = {
  {tern::LoadRecTy,  1,    (void*)0xdeadbeef, 12345},
  {tern::StoreRecTy, 3,    (void*)0xbeefdead, 67890},
  {tern::LoadRecTy,  5,    (void*)0xadbdeeef, 34567},
  {tern::StoreRecTy, 100,  (void*)0xdeafdbee, 1},
  {tern::LoadRecTy,  10000,(void*)0xefdeadbe, 2557555555},
};

struct call {
  unsigned insid;
  void *func;
  short narg;
  uint64_t data;
  uint64_t args[10];
};

call calls[5] = {
  {1,    (void*)(intptr_t)getpid, 0, 2000},
  {3,    (void*)(intptr_t)read,   3, 100, {10, 0xdeadbeef, 100}},
  {5,    (void*)(intptr_t)write,  3, 100, {80, 0xbeefdead, 400}},
  {100,  (void*)(intptr_t)mmap,   6, 0xefdeadbe, {0, 10000, PROT_WRITE|PROT_READ, MAP_SHARED, 20, 10000}},
  {10000,(void*)(intptr_t)strcpy, 2, 0xadbeefde, {0x88888800, 0x99999900}},
};

static inline
std::ostream& operator<<(std::ostream& os, const Log::reverse_rec_iterator& ri) {
  return os << &*ri;
}

static inline
std::ostream& operator<<(std::ostream& os, const Log::reverse_iterator& ri) {
  return os << &*ri;
}

TEST(recordertest, rec_iterator) {
  const unsigned num = 10000;

  tern_log_init();
  tern_log_thread_init(0);

  for(unsigned j=0; j<num; ++j) {
    unsigned i = j % (sizeof(loadstores)/sizeof(loadstores[0]));
    switch(loadstores[i].type) {
    case LoadRecTy:
      tern_log_load(loadstores[i].insid, loadstores[i].addr, loadstores[i].data);
      break;
    case StoreRecTy:
      tern_log_store(loadstores[i].insid, loadstores[i].addr, loadstores[i].data);
      break;
    }
  }

  tern_log_thread_exit();
  tern_log_exit();

  Log log(tern_log_name());
  Log::rec_iterator ri, rj;

  ri = log.rec_begin();
  ri += 10;
  rj = ri + 100;
  rj -= 50;
  ri = rj - 50;
  EXPECT_LT(ri, rj);
  EXPECT_LE(ri, rj);
  EXPECT_GT(rj, ri);
  EXPECT_GE(rj, ri);

  for(int i=0; i<50; ++i)
    ri ++;
  EXPECT_EQ(ri, rj);
  EXPECT_LE(ri, rj);
  EXPECT_GE(ri, rj);

  InsidRec *recp = ri;
  InsidRec &recref = *ri;
  EXPECT_EQ(recp, &recref);
  EXPECT_EQ(recp->insid, ri->insid);

  Log::reverse_rec_iterator rri, rrj;

  rri = log.rec_rbegin();
  rri += 10;
  rrj = rri + 100;
  rrj -= 50;
  rri = rrj - 50;

  EXPECT_LT(rri, rrj);
  EXPECT_LE(rri, rrj);
  EXPECT_GT(rrj, rri);
  EXPECT_GE(rrj, rri);

  for(int i=0; i<50; ++i)
    rri ++;
  EXPECT_EQ(rri, rrj);
  EXPECT_LE(rri, rrj);
  EXPECT_GE(rri, rrj);

  InsidRec *rrecp = &*rri;
  InsidRec &rrecref = *rri;
  EXPECT_EQ(rrecp, &rrecref);
  EXPECT_EQ(rrecp->insid, rri->insid);
}


TEST(recordertest, iterator) {
  const unsigned num = 10000;

  tern_log_init();
  tern_log_thread_init(0);

  for(unsigned j=0; j<num; ++j) {
    unsigned i = j % (sizeof(loadstores)/sizeof(loadstores[0]));
    switch(loadstores[i].type) {
    case LoadRecTy:
      tern_log_load(loadstores[i].insid, loadstores[i].addr, loadstores[i].data);
      break;
    case StoreRecTy:
      tern_log_store(loadstores[i].insid, loadstores[i].addr, loadstores[i].data);
      break;
    }
  }

  tern_log_thread_exit();
  tern_log_exit();

  Log log(tern_log_name());
  Log::iterator ii, ij;
}


TEST(recordertest, loadstore) {

  const unsigned num = 10000;

  tern_log_init();
  tern_log_thread_init(0);

  for(unsigned j=0; j<num; ++j) {
    unsigned i = j % (sizeof(loadstores)/sizeof(loadstores[0]));
    switch(loadstores[i].type) {
    case LoadRecTy:
      tern_log_load(loadstores[i].insid, loadstores[i].addr, loadstores[i].data);
      break;
    case StoreRecTy:
      tern_log_store(loadstores[i].insid, loadstores[i].addr, loadstores[i].data);
      break;
    }
  }

  tern_log_thread_exit();
  tern_log_exit();

  Log log(tern_log_name());

  Log::rec_iterator ri;
  unsigned j;
  for(ri=log.rec_begin(), j=0; ri!=log.rec_end(); ++ri, ++j) {
    unsigned i = j % (sizeof(loadstores)/sizeof(loadstores[0]));
    InsidRec *recp = ri;
    LoadRec *load;
    StoreRec *store;
    EXPECT_EQ(ri->type, loadstores[i].type);
    EXPECT_EQ(ri->insid, loadstores[i].insid);
    switch(ri->type) {
    case LoadRecTy:
      load = (LoadRec*)recp;
      EXPECT_EQ(load->addr, loadstores[i].addr);
      EXPECT_EQ(load->data, loadstores[i].data);
      break;
    case StoreRecTy:
      store = (StoreRec*)recp;
      EXPECT_EQ(store->addr, loadstores[i].addr);
      EXPECT_EQ(store->data, loadstores[i].data);
      break;
    }
  }

  Log::reverse_rec_iterator rri;
  for(rri=log.rec_rbegin(), j=num; rri!=log.rec_rend(); ++rri, --j) {
    unsigned i = (j-1) % (sizeof(loadstores)/sizeof(loadstores[0]));
    InsidRec *recp = &*rri;
    LoadRec *load;
    StoreRec *store;
    EXPECT_EQ(recp->type, loadstores[i].type);
    EXPECT_EQ(recp->insid, loadstores[i].insid);
    switch(recp->type) {
    case LoadRecTy:
      load = (LoadRec*)recp;
      EXPECT_EQ(load->addr, loadstores[i].addr);
      EXPECT_EQ(load->data, loadstores[i].data);
      break;
    case StoreRecTy:
      store = (StoreRec*)recp;
      EXPECT_EQ(store->addr, loadstores[i].addr);
      EXPECT_EQ(store->data, loadstores[i].data);
      break;
    }
  }

}


TEST(recordertest, call) {

  const unsigned num = 10000;

  tern_log_init();
  tern_log_thread_init(0);

  for(unsigned j=0; j<num; ++j) {
    unsigned i = j % (sizeof(calls)/sizeof(calls[0]));
    switch(calls[i].narg) {
    case 0:
      tern_log_call(0, calls[i].insid, 0, calls[i].func);
      break;
    case 1:
      tern_log_call(0, calls[i].insid, 1, calls[i].func, calls[i].args[0]);
      break;
    case 2:
      tern_log_call(0, calls[i].insid, 2, calls[i].func, calls[i].args[0],
                    calls[i].args[1]);
      break;
    case 3:
      tern_log_call(0, calls[i].insid, 3, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2]);
      break;
    case 4:
      tern_log_call(0, calls[i].insid, 4, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2], calls[i].args[3]);
      break;
    case 5:
      tern_log_call(0, calls[i].insid, 5, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2], calls[i].args[3],
                    calls[i].args[4]);
      break;
    case 6:
      tern_log_call(0, calls[i].insid, 6, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2], calls[i].args[3],
                    calls[i].args[4], calls[i].args[5]);
      break;
    case 7:
      tern_log_call(0, calls[i].insid, 7, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2], calls[i].args[3],
                    calls[i].args[4], calls[i].args[5], calls[i].args[6]);
      break;
    case 8:
      tern_log_call(0, calls[i].insid, 8, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2], calls[i].args[3],
                    calls[i].args[4], calls[i].args[5], calls[i].args[6],
                    calls[i].args[7]);
      break;
    case 9:
      tern_log_call(0, calls[i].insid, 9, calls[i].func, calls[i].args[0],
                    calls[i].args[1], calls[i].args[2], calls[i].args[3],
                    calls[i].args[4], calls[i].args[5], calls[i].args[6],
                    calls[i].args[7], calls[i].args[8]);
    }
    tern_log_ret(0, calls[i].insid, calls[i].narg, calls[i].func, calls[i].data);
  }

  tern_log_thread_exit();
  tern_log_exit();

  Log log(tern_log_name());

  Log::rec_iterator ri;
  unsigned i = 0;
  for(ri=log.rec_begin(); ri!=log.rec_end(); ++ri) {
    InsidRec *recp = ri;
    CallRec *call;
    ExtraArgsRec *extra;
    ReturnRec *ret;

    switch(ri->type) {
    case CallRecTy: {
      short argnr;
      short rec_narg;
      short rec_argnr;
      short seq = 0;
      short nextra;

      call = (CallRec*)recp;
      EXPECT_EQ(call->insid, calls[i].insid);
      EXPECT_EQ(call->seq, seq);
      EXPECT_EQ(call->narg, calls[i].narg);
      EXPECT_EQ(call->func, calls[i].func);

      // inline args
      rec_narg = std::min(calls[i].narg, (short)MAX_INLINE_ARGS);
      for(argnr=0; argnr<rec_narg; ++argnr)
        EXPECT_EQ(call->args[argnr], calls[i].args[argnr]);

      // other args
      nextra = NumExtraArgsRecords(calls[i].narg);
      Log::rec_iterator r = ri + 1;
      for(++seq; seq<=nextra; ++seq, ++r) {
        extra = (ExtraArgsRec*)&*r;
        EXPECT_EQ(extra->insid, calls[i].insid);
        EXPECT_EQ(extra->type, ExtraArgsRecTy);
        EXPECT_EQ(extra->seq, seq);
        EXPECT_EQ(extra->narg, calls[i].narg);
        rec_narg = std::min(extra->narg, (short)(MAX_INLINE_ARGS+MAX_EXTRA_ARGS*seq));

        for(; argnr<rec_narg; ++argnr) {
          rec_argnr = argnr - MAX_INLINE_ARGS-MAX_EXTRA_ARGS*(seq-1);
          EXPECT_EQ(extra->args[rec_argnr], calls[i].args[argnr]);
        }
      }
      // all narg args should have been checked
      EXPECT_EQ(argnr, calls[i].narg);

      // return
      ret = (ReturnRec*)&*r;
      EXPECT_EQ(ret->insid, calls[i].insid);
      EXPECT_EQ(ret->type, ReturnRecTy);
      EXPECT_EQ(ret->seq, seq);
      EXPECT_EQ(ret->narg, calls[i].narg);
      EXPECT_EQ(ret->func, calls[i].func);
      EXPECT_EQ(ret->data, calls[i].data);
    }
      i = (i+1) % (sizeof(calls)/sizeof(calls[0]));  // next call in calls
      break;
    }
  }
}


