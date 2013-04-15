#ifndef __SPEC_HOOK_FUNC_NAME
extern "C" void FUNC_NAME(ARGS_WITH_NAME){
  typedef int (*orig_func_type)(ARGS_WITHOUT_NAME);

  static orig_func_type orig_func;

  void * handle;

  if (!orig_func) {
    if(!(handle=dlopen("LIB_PATH", RTLD_LAZY))) {
      perror("dlopen");
      abort();
    }

    orig_func = (orig_func_type) dlsym(handle, "FUNC_NAME");

    if(dlerror()) {
      perror("dlsym");
      abort();
    }

    dlclose(handle);
  }

#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT) {

#ifdef __NEED_INPUT_INSID
    void *eip = 0;
    if (options::dync_geteip) {
      Space::enterSys();
      eip = get_eip();
      Space::exitSys();
    }
    tern_FUNC_NAME((unsigned)(uint64_t) eip, ARGS_ONLY_NAME);
#else
    tern_FUNC_NAME(ARGS_ONLY_NAME);
#endif

#ifdef PRINT_DEBUG
    fprintf(stdout, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
#endif
  }
#else
#endif

#ifdef PRINT_DEBUG
  fprintf(stdout, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
#endif

  orig_func(ARGS_ONLY_NAME);
}
#endif
