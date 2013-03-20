c----------------------------------------------------------
c----- Fortran Wrapper for xtern annotations
c----- ( iso_c_binding requires Fortran 2000+ compiler
c-----   e.g. gcc 4.3 + )
c----------------------------------------------------------
    
      interface
          subroutine tern_lineup_init( opaque_type, count,
     &               timeout_turns) bind ( c )
              use iso_c_binding
              integer(kind=c_int), VALUE :: opaque_type
              integer(kind=c_int), VALUE :: count
              integer(kind=c_int), VALUE :: timeout_turns
          end subroutine tern_lineup_init
          subroutine tern_lineup( opaque_type) bind ( c )
              use iso_c_binding
              integer(kind=c_int), VALUE :: opaque_type
          end subroutine tern_lineup
          subroutine tern_non_det_start() bind ( c )
              use iso_c_binding
          end subroutine tern_non_det_start
          subroutine tern_non_det_end() bind ( c )
              use iso_c_binding
          end subroutine tern_non_det_end
	  subroutine tern_non_det_barrier_end(bar_id, cnt) bind ( c )
              use iso_c_binding
              integer(kind=c_int), VALUE :: bar_id
              integer(kind=c_int), VALUE :: cnt
          end subroutine tern_non_det_barrier_end
      end interface
