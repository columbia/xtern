#ifndef __TERN_PATH_SLICER_DBG_PRINT_H
#define __TERN_PATH_SLICER_DBG_PRINT_H

#define DBG0 0 /* No dbg print. */
#define DBG1 1 /* Dbg print everything. */
#define DBG2 2 /* Dbg print only some important events. */

#define SERRS if(get_option(tern_path_slicer,print_debug_info)==DBG1)errs()
#define SERRS2 if(get_option(tern_path_slicer,print_debug_info)==DBG2)errs()

#define GETTIME(clock) gettimeofday(&clock,NULL)
#define ADDTIME(num, clock_st, clock_end) \
  num+=(clock_end.tv_sec-clock_st.tv_sec)+(clock_end.tv_usec-clock_st.tv_usec)/1000000.0
#define BEGINTIME(clock) GETTIME(clock)
#define ENDTIME(num, clock_st, clock_end) GETTIME(clock_end);ADDTIME(num, clock_st, clock_end)

#define NORMAL_SLICING (get_option(tern_path_slicer,slicing_mode)==0)
#define MAX_SLICING (get_option(tern_path_slicer,slicing_mode)==1)
#define RANGE_SLICING (get_option(tern_path_slicer, slicing_mode)==2)

#endif

