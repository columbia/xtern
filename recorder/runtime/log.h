/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOG_H
#define __TERN_RECORDER_LOG_H

extern "C" {
  void tern_log_i8(int insid, void* addr, unsigned char data);  
  void tern_log_i16(int insid, void* addr, unsigned short data);
  void tern_log_i32(int insid, void* addr, unsigned int data);
  void tern_log_i64(int insid, void* addr, unsigned long long data);
  void tern_log_init(void);
  void tern_log_exit(void);
}

#endif
