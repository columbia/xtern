#ifndef __SPEC_HOOK_FUNC_NAME
extern "C" void FUNC_NAME(ARGS_WITH_NAME){
  typedef int (*orig_func_type)(ARGS_WITHOUT_NAME);

  orig_func_type orig_func;
  void * handle;
  void *eip = 0;

  if(!(handle=dlopen("LIB_PATH", RTLD_LAZY))) {
    perror("dlopen");
    puts("here dlopen");
    abort();
  }

  orig_func = (orig_func_type) dlsym(handle, "FUNC_NAME");

  if(dlerror()) {
    perror("dlsym");
    puts("here dlsym");
    abort();
  }

  dlclose(handle);

#ifdef __USE_TERN_RUNTIME
  if (Space::isApp()) {
    if (options::DMT) {
      //fprintf(stderr, "Parrot hook: pid %d self %u calls %s\n", getpid(), (unsigned)pthread_self(), "FUNC_NAME");
#ifdef __NEED_INPUT_INSID
      if (options::dync_geteip) {
        Space::enterSys();
        eip = get_eip();
        Space::exitSys();
      }
      record_rdtsc_op("FUNC_NAME", "START", 0, eip);
      tern_FUNC_NAME((unsigned)(uint64_t) eip, ARGS_ONLY_NAME);
#else
      tern_FUNC_NAME(ARGS_ONLY_NAME);
#endif
      record_rdtsc_op("FUNC_NAME", "END", 0, eip);
      return; 
    } else {// For performance debugging, by doing this, we are still able to get the sync wait time for non-det mode.
      if (options::dync_geteip) {
        Space::enterSys();
        eip = get_eip();
        Space::exitSys();
      }
      record_rdtsc_op("FUNC_NAME", "START", 0, eip);
      Space::enterSys();
      orig_func(ARGS_ONLY_NAME);
      Space::exitSys();
      record_rdtsc_op("FUNC_NAME", "END", 0, eip);
      return;
    }
  }
#else
#endif

  orig_func(ARGS_ONLY_NAME);
}
#endif
