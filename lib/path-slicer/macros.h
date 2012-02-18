#ifndef __TERN_PATH_SLICER_MACROS_H
#define __TERN_PATH_SLICER_MACROS_H

#define DBG0 0 /* No dbg print. */
#define DBG1 1 /* Dbg print everything. */
#define DBG2 2 /* Dbg print only some important events. */

#if __WORDSIZE == 64
#define U64 "%lu"
#define X64 "%lx"
#define SZ "%lu"
#define UU "%u"
#else
#define U64 "%llu"
#define X64 "%llx"
#define SZ "%u"
#define UU "%u"
#endif

#include "options.h"
#include "path_slicer-options.h"
#define SERRS if(get_option(tern_path_slicer,print_debug_info)==DBG1)errs()
#define SERRS2 if(get_option(tern_path_slicer,print_debug_info)==DBG2)errs()
#define DPRINT(args...) if(get_option(tern,slicer_print_debug_info)==1)fprintf(stderr, args)
#define DBG (get_option(tern_path_slicer,print_debug_info)!=0)

#define BAN "\n\n\n\n====================================================================================================\n\n\n\n"
#define EXPR_BEGIN "\n\n----------TERN EXPR BEGIN----------\n";
#define EXPR_END "\n\n----------TERN EXPR END----------\n";
#define LLVM_CHECK_TAG "// CHECK: "

#define GETTIME(clock) gettimeofday(&clock,NULL)
#define ADDTIME(num, clock_st, clock_end) \
  num+=(clock_end.tv_sec-clock_st.tv_sec)+(clock_end.tv_usec-clock_st.tv_usec)/1000000.0
#define BEGINTIME(clock) GETTIME(clock)
#define ENDTIME(num, clock_st, clock_end) GETTIME(clock_end);ADDTIME(num, clock_st, clock_end)

#define INTRA_SLICING_FOR_TEST (get_option(tern_path_slicer,do_intra_slicing_at_last_instr)==1)

#define NORMAL_SLICING (get_option(tern_path_slicer,slicing_mode)==0)
#define MAX_SLICING (get_option(tern_path_slicer,slicing_mode)==1)
#define RANGE_SLICING (get_option(tern_path_slicer, slicing_mode)==2)

#define KLEE_RECORDING (get_option(tern_path_slicer, trace_util_type)==0)
#define XTERN_RECORDING (get_option(tern_path_slicer, trace_util_type)==1)

#define SIZE_T_INVALID (size_t(-1))
#define U_NEG1 (unsigned(-1))
#define ASSERT(stmt) if(get_option(tern_path_slicer,print_debug_info)!=DBG0)assert(stmt)

#define CTX_SENSITIVE (get_option(tern_path_slicer, context_sensitive_ailas_query)==1)

#include <ext/hash_set>
#include <ext/hash_map>
#define HMAP __gnu_cxx::hash_map
#define HM_IN(ELEM, SET) (SET.find(ELEM) != SET.end())
#define DM_IN(ELEM, SET) (SET.count(ELEM) > 0)
#define DS_IN(ELEM, SET) DM_IN(ELEM, SET)

#include "taken-flags.h"

#endif

