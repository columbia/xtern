/* -*- Mode: C++ -*- */

#ifndef __XTERN_RECORDER_LOG_H
#define __XTERN_RECORDER_LOG_H

extern "C" {
  void xtern_log_i8(int insid, void* addr, unsigned char data);  
  void xtern_log_i16(int insid, void* addr, unsigned short data);
  void xtern_log_i32(int insid, void* addr, unsigned int data);
  void xtern_log_i64(int insid, void* addr, unsigned long long data);
  void xtern_log_init(void);
  void xtern_log_exit(void);
}

#endif
