#include "trace-util.h"
using namespace tern;

TraceUtil::TraceUtil() {

}

TraceUtil::~TraceUtil() {

}

void TraceUtil::initCallStackMgr(CallStackMgr *ctxMgr) {
  this->ctxMgr = ctxMgr;
}

