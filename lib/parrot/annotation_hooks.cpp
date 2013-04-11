#ifndef __SPEC_HOOK_tern_lineup_init
extern "C" void soba_init(long opaque_type, unsigned count, unsigned timeout_turns){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_init_real(opaque_type, count, timeout_turns);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_lineup_destroy
extern "C" void soba_destroy(long opaque_type){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_destroy_real(opaque_type);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_lineup_start
extern "C" void tern_lineup_start(long opaque_type){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_start_real(opaque_type);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_lineup_end
extern "C" void tern_lineup_end(long opaque_type){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_end_real(opaque_type);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_lineup
extern "C" void soba_wait(long opaque_type){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_start_real(opaque_type);
    tern_lineup_end_real(opaque_type);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_non_det_start
extern "C" void pcs_enter(){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations && options::enforce_non_det_annotations) {
    tern_non_det_start_real();
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_non_det_end
extern "C" void pcs_exit(){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations && options::enforce_non_det_annotations) {
    tern_non_det_end_real();
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_set_base_time
extern "C" void tern_set_base_timespec(struct timespec *ts){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_set_base_time_real(ts);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_set_base_time
extern "C" void tern_set_base_timeval(struct timeval *tv){
#ifdef __USE_TERN_RUNTIME
  struct timespec ts;
  ts.tv_sec = tv->tv_sec;
  ts.tv_nsec = tv->tv_usec * 1000;
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_set_base_time_real(&ts);
  }
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_detach
extern "C" void tern_detach(){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_detach_real();
  }
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_non_det_barrier_end
extern "C" void pcs_barrier_exit(int bar_id, int cnt){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations && options::enforce_non_det_annotations) {
    tern_non_det_barrier_end_real(bar_id, cnt);
  }
#endif
  // If not runnning with xtern, NOP.
}
#endif
