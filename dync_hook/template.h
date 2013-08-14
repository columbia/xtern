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
FILE *rdtsc_log = NULL;

void record_rdtsc_op(const char *op_name, const char *op_suffix, int print_depth) {
  if (options::record_rdtsc) {
    if (rdtsc_log == NULL) {
      char log_path[1024];
      memset(log_path, 0, 1024);
      snprintf(log_path, 1024, "%s/pself-pid-%u.txt", options::rdtsc_output_dir.c_str(), (unsigned)getpid());
      mkdir(options::rdtsc_output_dir.c_str(), 0777);
      rdtsc_log = fopen(log_path, "w");
      assert(rdtsc_log);
    }
    for (int i = 0; i < print_depth; i++)
      fprintf(rdtsc_log, "	");
    fprintf(rdtsc_log, "%u %s %s %llu\n", (unsigned)pthread_self(), op_name, op_suffix, rdtsc());
  }
  //fprintf(stderr, "%u : %s %s %llu\n", (unsigned)pthread_self(), op_name, op_suffix, rdtsc());
}

