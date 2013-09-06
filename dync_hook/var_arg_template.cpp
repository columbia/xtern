#ifndef __SPEC_HOOK_fcntl
extern "C" int fcntl(int fd, int cmd, ...){
  typedef int (*orig_func_type)(int , int , ...);
  static orig_func_type orig_func;

  void * handle;
  int ret;
  void *eip = 0;

  if (!orig_func) {
    if(!(handle=dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY))) {
      perror("dlopen");
      puts("here dlopen");
      abort();
    }

    orig_func = (orig_func_type) dlsym(handle, "fcntl");

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
      record_rdtsc_op("fcntl", "START", 0, eip);
      va_list arg_list;
      va_start(arg_list, cmd);
      ret = tern_fcntl((unsigned)(uint64_t) eip, fd, cmd, arg_list);
      va_end(arg_list);
#else
      va_list arg_list;
      va_start(arg_list, cmd);
      ret = tern_fcntl(fd, cmd, arg_list);
      va_end(arg_list);
#endif
      record_rdtsc_op("fcntl", "END", 0, eip);
      return ret;
    } else {// For performance debugging, by doing this, we are still able to get the sync wait time for non-det mode.
      if (options::dync_geteip) {
        Space::enterSys();
        eip = get_eip();
        Space::exitSys();
      }
      record_rdtsc_op("fcntl", "START", 0, eip);
      Space::enterSys();
      va_list arg_list;
      va_start(arg_list, cmd);
      ret = orig_func(fd, cmd, arg_list);
      va_end(arg_list);
      Space::exitSys();
      record_rdtsc_op("fcntl", "END", 0, eip);
      return ret;
    }
  } 
#endif

  va_list arg_list;
  va_start(arg_list, cmd);
  ret = orig_func(fd, cmd, arg_list);
  va_end(arg_list);

  return ret;
}
#endif
