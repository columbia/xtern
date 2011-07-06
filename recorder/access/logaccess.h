/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGACCESS_H
#define __TERN_RECORDER_LOGACCESS_H

#include "recorder/runtime/logdefs.h"

namespace tern
{
  struct Log {

    struct Insid
    {
      unsigned type   : 3;
      unsigned insid  :29;
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Load: public Insid
    {
      void*    addr;
      uint64_t data;
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Store: public Insid
    {
      void*    addr;
      uint64_t data;
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Call: public Insid
    {
      short    seq;
      short    narg;
      void*    func;
      unit64_t args[NUM_INLINE_ARGS];
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct ExtraArgs: public Insid
    {
      short    seq;
      short    narg;
      uint64_t args[NUM_EXTRA_ARGS];
    } __attribute__ ((aligned (RECORD_SIZE)));

    struct Return: public Insid
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
