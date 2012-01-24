#ifdef __SPEC_HOOK___libc_start_main

extern "C" void __tern_prog_begin(void);  //  lib/runtime/helper.cpp
extern "C" void __tern_prog_end(void); //  lib/runtime/helper.cpp

typedef int (*main_type)(int, char**, char**);

struct arg_type
{
  char **argv;
  int (*main_func) (int, char **, char **);
};

extern "C" int my_main(int argc, char **pt, char **aa)
{
  int ret;
  arg_type *args = (arg_type*)pt;
  __tern_prog_begin();
	fprintf(stderr, "%04d: __libc_start_main() called.\n", (int) pthread_self());
  ret = args->main_func(argc, args->argv, aa);
	fprintf(stderr, "%04d: __tern_prog_end() called.\n", (int) pthread_self());
  fflush(stderr);
  __tern_prog_end();
  return ret;
}

extern "C" int __libc_start_main(
  void *func_ptr, 
  int argc, 
  char* argv[], 
  void *init_func,
  void *fini_func, 
  void *stack_end)
{
	typedef int (*orig_func_type)(void *, int, char *[], void*, void*, void*);
  
	orig_func_type orig_func;
  arg_type args;

	void * handle;
	int ret;

	if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
		puts("dlopen error");
		abort();
	}

	orig_func = (orig_func_type) dlsym(handle, "__libc_start_main");

	if(dlerror()) {
		puts("dlerror");
		abort();
	}

	dlclose(handle);

#ifdef __USE_TERN_RUNTIME
#ifdef PRINT_DEBUG
	fprintf(stderr, "%04d: __libc_start_main is hooked.\n", (int) pthread_self());
#endif
  //fprintf(stderr, "%04d: __tern_prog_begin() called.\n", (int) pthread_self());
  args.argv = argv;
  args.main_func = (main_type)func_ptr;
  ret = orig_func((void*)my_main, argc, (char**)(&args), init_func, fini_func, stack_end);
  return ret;
#endif
	ret = orig_func(func_ptr, argc, argv, init_func, fini_func, stack_end);

	return ret;
}
#endif

