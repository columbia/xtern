#ifndef __SPEC_HOOK_tern_lineup_init
extern "C" void tern_lineup_init(long opaque_type, unsigned count, unsigned timeout_turns){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_init_real(opaque_type, count, timeout_turns);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

#ifndef __SPEC_HOOK_tern_lineup_destroy
extern "C" void tern_lineup_destroy(long opaque_type){
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
extern "C" void tern_lineup(long opaque_type){
#ifdef __USE_TERN_RUNTIME
  if (Space::isApp() && options::DMT && options::enforce_annotations) {
    tern_lineup_start_real(opaque_type);
    tern_lineup_end_real(opaque_type);
  } 
#endif
  // If not runnning with xtern, NOP.
}
#endif

