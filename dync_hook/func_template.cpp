#ifndef __SPEC_HOOK_FUNC_NAME
extern "C" FUNC_RET_TYPE FUNC_NAME(ARGS_WITH_NAME){
	typedef int (*orig_func_type)(ARGS_WITHOUT_NAME);
  
	orig_func_type orig_func;

	void * handle;
	FUNC_RET_TYPE ret;

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
  if (need_hook) {
    need_hook = false;
#ifdef __NEED_INPUT_INSID
  	ret = tern_FUNC_NAME(0, ARGS_ONLY_NAME);
#else
  	ret = tern_FUNC_NAME(ARGS_ONLY_NAME);
#endif
    need_hook = true;

#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
#endif
  } else
#else
#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
#endif
#endif
  	ret = orig_func(ARGS_ONLY_NAME);

	return ret;
}
#endif
