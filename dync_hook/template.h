#include <list>

// Only support i386 and x86_64.
#if defined(__i386__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

//__thread
// per process.
struct sync_op_entry {
  unsigned tid;
  std::string op;
  std::string op_suffix;
  unsigned long long clock;
  sync_op_entry() {
    tid = 0;
    op = "";
    op_suffix = "";
    clock = 0;
  }
};

pthread_spinlock_t rdtsc_lock;
std::list<sync_op_entry *> rdtsc_log;

void process_rdtsc_log(void) {
  printf("Storing rdtsc log...\n");
  char log_path[1024];
  memset(log_path, 0, 1024);
  snprintf(log_path, 1024, "%s/pself-pid-%u.txt", options::rdtsc_output_dir.c_str(), (unsigned)getpid());
  mkdir(options::rdtsc_output_dir.c_str(), 0777);
  FILE *f = fopen(log_path, "w");
  assert(f);

  std::list<sync_op_entry *>::iterator itr = rdtsc_log.begin();
  for (; itr != rdtsc_log.end(); itr++) {
    struct sync_op_entry *entry = *itr;
    fprintf(f, "%u %s %s %llu\n", entry->tid, entry->op.c_str(), entry->op_suffix.c_str(), entry->clock);
    delete entry;
  }
  rdtsc_log.clear();
}

void record_rdtsc_op(const char *op_name, const char *op_suffix, int print_depth) {
  if (options::record_rdtsc) {
    if (rdtsc_log.size() == 0) {
      pthread_spin_init(&rdtsc_lock, 0);
      atexit(process_rdtsc_log);
    }

    pthread_spin_lock(&rdtsc_lock);
    struct sync_op_entry *entry = new struct sync_op_entry;    
    entry->tid = (unsigned)pthread_self();
    for (int i = 0; i < print_depth; i++)
      entry->op += "	";
    entry->op += op_name;
    entry->op_suffix = op_suffix;
    entry->clock = rdtsc();
    //fprintf(rdtsc_log, "%u %s %s %llu\n", (unsigned)pthread_self(), op_name, op_suffix, rdtsc());
    rdtsc_log.push_back(entry);
    pthread_spin_unlock(&rdtsc_lock);
  }
  //fprintf(stderr, "%u : %s %s %llu\n", (unsigned)pthread_self(), op_name, op_suffix, rdtsc());
}

