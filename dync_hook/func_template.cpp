#ifndef __SPEC_HOOK_FUNC_NAME
extern "C" FUNC_RET_TYPE FUNC_NAME(ARGS_WITH_NAME){
  typedef FUNC_RET_TYPE (*orig_func_type)(ARGS_WITHOUT_NAME);

  static orig_func_type orig_func;

  void * handle;
  FUNC_RET_TYPE ret;
  void *eip = 0;

  if (!orig_func) {
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
  }

#ifdef __USE_TERN_RUNTIME
  if (Space::isApp()) {
    if (options::DMT) {
#ifdef __NEED_INPUT_INSID
      if (options::dync_geteip) {
        Space::enterSys();
        eip = get_eip();
        Space::exitSys();
      }
      record_rdtsc_op("FUNC_NAME", "START", 0, eip);
      ret = tern_FUNC_NAME((unsigned)(uint64_t) eip, ARGS_ONLY_NAME);
#else
      ret = tern_FUNC_NAME(ARGS_ONLY_NAME);
#endif
      record_rdtsc_op("FUNC_NAME", "END", 0, eip);
      return ret;
    } else {// For performance debugging, by doing this, we are still able to get the sync wait time for non-det mode.
      if (options::dync_geteip) {
        Space::enterSys();
        eip = get_eip();
        Space::exitSys();
      }
      record_rdtsc_op("FUNC_NAME", "START", 0, eip);
      Space::enterSys();
      ret = orig_func(ARGS_ONLY_NAME);
      Space::exitSys();
      record_rdtsc_op("FUNC_NAME", "END", 0, eip);
      return ret;
    }
  } 
#endif

  ret = orig_func(ARGS_ONLY_NAME);

  return ret;
}
#endif

