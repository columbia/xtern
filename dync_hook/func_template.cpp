#ifndef __SPEC_HOOK_FUNC_NAME
extern "C" FUNC_RET_TYPE FUNC_NAME(ARGS_WITH_NAME){
  typedef FUNC_RET_TYPE (*orig_func_type)(ARGS_WITHOUT_NAME);

#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT) {
    FUNC_RET_TYPE ret;

#ifdef PRINT_DEBUG
    fprintf(stdout, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
    print_stack();
    fflush(stdout);
#endif

#ifdef __NEED_INPUT_INSID
    Space::enterSys();
    void *eip = options::dync_geteip ? get_eip() : 0;
    Space::exitSys();
    ret = tern_FUNC_NAME((uintptr_t) eip, ARGS_ONLY_NAME);
#else
    ret = tern_FUNC_NAME(ARGS_ONLY_NAME);
#endif

#ifdef PRINT_DEBUG
    fprintf(stdout, "%04d: FUNC_NAME returned.\n", (int) pthread_self());
    fflush(stdout);
#endif
    return ret;
  } 
#endif

#ifdef PRINT_DEBUG
  fprintf(stdout, "%04d: FUNC_NAME is called.\n", (int) pthread_self());
  fflush(stdout);
#endif

  static orig_func_type orig_func = NULL;
  if (!orig_func) {
    void * handle;
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

  return orig_func(ARGS_ONLY_NAME);
}
#endif

