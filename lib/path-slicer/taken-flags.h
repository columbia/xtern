#ifndef __TERN_PATH_SLICER_TAKEN_FLAGS_H
#define __TERN_PATH_SLICER_TAKEN_FLAGS_H

#define NUM_TAKEN_FLAGS 256
extern const char *takenReasons[NUM_TAKEN_FLAGS];

/* Flags to identify whether a dynamic instruction is taken, and reasons of taken. */
#define NOT_TAKEN 0

/* Reasons of taken by handling important events (star target or buggy points, synch calls, real races). */
#define START_TARGET 1
#define TAKEN_EVENT 2
#define TAKEN_RACE 3
// TBD

#define INTER_PHASE_BASE 29  /* Base of inter-thread phase. */
/* Reasons of taken by handling inter-thread phase. */
/* Reasons of taken by handling instruction -instruction may race in inter-thread phase. */
#define INTER_INSTR2 30
#define INTER_LOAD_TGT 31
#define INTER_STORE_TGT 32
// TBD

/* Reasons of taken by handling branch -instruction may race in inter-thread phase. */
#define INTER_BR_INSTR 40
// TBD

/* Reasons of taken by handling branch -branch may race in inter-thread phase. */
#define INTER_BR_BR 50
// TBD

#define INTRA_PHASE_BASE 99  /* Base of intra-thread phase. */
/* Reasons of taken by handling intra-thread phase. */
/* Reasons of taken by handling PHI instructions. */
#define INTRA_PHI 100
#define INTRA_PHI_BR_CTRL_DEP 101
// TBD

/* Reasons of taken by handling branch instructions. */
#define INTRA_BR 110
// TBD

/* Reasons of taken by handling return instructions. */
//#define INTRA_RET 120
#define INTRA_RET_REG_OW 121 /* OVERWRITE. */
#define INTRA_RET_CALL_EVENT 122 /* Calling event only. */
#define INTRA_RET_WRITE_FUNC 123 /* Write func only. */
#define INTRA_RET_BOTH 124 /* Both calling event and writing func. */
// TBD

/* Reasons of taken by handling call instructions. */
#define INTRA_CALL 130
// TBD

/* Reasons of taken by handling load instructions. */
//#define INTRA_LOAD 140
#define INTRA_LOAD_OW 141
// TBD

/* Reasons of taken by handling store instructions. */
//#define INTRA_STORE 150
#define INTRA_STORE_OW 151 /* OVERWRITE. */
#define INTRA_STORE_ALIAS 152
// TBD

/* Reasons of taken by handling all other non memory instructions. */
#define INTRA_NON_MEM 160
// TBD

#endif

