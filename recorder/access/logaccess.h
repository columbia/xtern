/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include "recorder/runtime/log.h"

namespace tern
{
  struct Log {

    struct Record
    {
      int insid;
    };

    struct Load: public Record
    {
      void* addr;
      uint64_t data;
    } __attribute__ ((aligned (LOG_ALIGN)));

    struct Store: public Record
    {
      void* addr;
      uint64_t data;
    } __attribute__ ((aligned (LOG_ALIGN)));

    struct Arg: public Record
    {
      void *addr
      int size;
      int type;
    } __attribute__ ((aligned (LOG_ALIGN)));

    struct Return: public Record
    {
      void* func;
      uint64_t data;
    } __attribute__ ((aligned (LOG_ALIGN)));
    
    struct iterator {};
    struct reverse_iterator {};
  };

  
}

#endif
