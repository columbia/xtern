#ifndef __SPEC_HOOK_FUNC_NAME
extern "C" void FUNC_NAME(ARGS_WITH_NAME){
	typedef int (*orig_func_type)(ARGS_WITHOUT_NAME);

	orig_func_type orig_func;

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

#ifdef __USE_TERN_RUNTIME
	if (Space::isApp()) {
    Space::enterSys();
    void *eip = options::dync_geteip ? get_eip() : 0;
    Space::exitSys();

#ifdef __NEED_INPUT_INSID
  	tern_FUNC_NAME((unsigned)(uint64_t) eip, ARGS_ONLY_NAME);
#else
  	tern_FUNC_NAME(ARGS_ONLY_NAME);
#endif

#ifdef PRINT_DEBUG
	fprintf(stdout, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
#endif
  } else
#else
#endif
  	orig_func(ARGS_ONLY_NAME);

#ifdef PRINT_DEBUG
	fprintf(stdout, "%04d: FUNC_NAME is hooked.\n", (int) pthread_self());
#endif
}
#endif
