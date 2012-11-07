#
# Makefile for OpenMP demo programs
#
# "make program" to make one program
# "make" or "make all" to make all executables
# "make clean" to remove executables
#

OMPC        = gcc
CFLAGS      = -O3 -fopenmp   ### -Wall -pedantic

OMPCC       = g++
CCFLAGS     = -O3 -fopenmp   ### -Wall -pedantic 

LFLAGS      = -lm 

ALL =   omp_hello \
	omp_num_threads \
	omp_shared  \
	omp_lastprivate  \
	omp_reduction  \
	omp_sections  \
	omp_critical \
	omp_for \
	omp_get_env_info \
	omp_threadprivate  \
	omp_matrixmult \
	omp_pi  \
	pi_rect  \
	pi_trap

.PHONY:  all
all:  $(ALL)

%: %.c 
	$(OMPC) $(CFLAGS) $< -o $@ $(LFLAGS)

.PHONY:  clean
clean:
	-rm $(ALL)

