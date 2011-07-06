/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include "recorder/runtime/log.h"

namespace tern
{
  struct Log {

    struct Record
    {
      int      insid;
    };

    struct Load: public Record
    {
      void*    addr;
      uint64_t data;
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Store: public Record
    {
      void*    addr;
      uint64_t data;
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Call: public Record
    {
      short    seq;
      short    narg;
      void*    func;
      unit64_t args[NUM_INLINE_ARGS];
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct ExtraArgs: public Record
    {
      short    seq;
      short    narg;
      uint64_t arg[NUM_EXTRA_ARGS];
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Return: public Record
    {
      short    seq;
      short    narg;
      void*    func;
      uint64_t data;
    } __attribute__ ((aligned (RECORD_SIZE)));
    
    struct iterator {};
    struct reverse_iterator {};
  };

  
}

#endif
