#include "tid.h"

namespace tern {

__thread int tid_manager::self_tid = -1;
tid_manager tids;

}
