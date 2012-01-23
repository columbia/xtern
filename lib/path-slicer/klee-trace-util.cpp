#include "klee-trace-util.h"
using namespace tern;

using namespace klee;

KleeTraceUtil::KleeTraceUtil() {

}

KleeTraceUtil::~KleeTraceUtil() {

}

void KleeTraceUtil::load(const char *tracePath, DynInstrVector *trace) {

}

void KleeTraceUtil::store(const char *tracePath) {

}

void KleeTraceUtil::record(void *instr, void *state) {
  record((KInstruction *)instr, (ExecutionState *)state);
}

void KleeTraceUtil::record(KInstruction *instr, ExecutionState *state) {
  // TBD.
}
