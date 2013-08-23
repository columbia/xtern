#include "tern/runtime/rdtsc.h"

using namespace std;

pthread_spinlock_t rdtsc_lock;
const size_t max_v_size = 100000000;
std::vector<sync_op_entry *> rdtsc_log;
size_t rdtsc_index = (size_t)-1;

void process_rdtsc_log(void) {
  printf("Storing rdtsc log...\n");
  char log_path[1024];
  memset(log_path, 0, 1024);
  snprintf(log_path, 1024, "%s/pself-pid-%u.txt", options::rdtsc_output_dir.c_str(), (unsigned)getpid());
  mkdir(options::rdtsc_output_dir.c_str(), 0777);
  FILE *f = fopen(log_path, "w");
  assert(f);

  for (size_t i = 0; i < rdtsc_index; i++) {
    struct sync_op_entry *entry = rdtsc_log[i];
    fprintf(f, "%u %s %s %llu %p\n", entry->tid, entry->op.c_str(), entry->op_suffix.c_str(), entry->clock, entry->eip);
    delete entry;
  }
  rdtsc_log.clear();
  fflush(f);
  fclose(f);
}

void record_rdtsc_op(const char *op_name, const char *op_suffix, int print_depth, void *eip) {
  if (options::record_rdtsc) {
    if (rdtsc_index == (size_t)-1) {
      pthread_spin_init(&rdtsc_lock, 0);
        printf("At exit...\n");
      atexit(process_rdtsc_log);
      rdtsc_log.reserve (max_v_size);
      rdtsc_index = 0;
    }

    struct sync_op_entry *entry = new struct sync_op_entry;    
    entry->tid = (unsigned)pthread_self();
    for (int i = 0; i < print_depth; i++)
      entry->op += "----"; // add print depth, just for easy looking.
    entry->op += op_name;
    entry->op_suffix = op_suffix;
    entry->eip = eip;

    pthread_spin_lock(&rdtsc_lock);
    entry->clock = rdtsc();
    assert(rdtsc_index < max_v_size && "Please pre-allocated a bigger vector size for rdtsc log.");
    rdtsc_log[rdtsc_index] = entry;
    rdtsc_index++;
    pthread_spin_unlock(&rdtsc_lock);
  }
  //fprintf(stderr, "%u : %s %s %llu\n", (unsigned)pthread_self(), op_name, op_suffix, rdtsc());
}