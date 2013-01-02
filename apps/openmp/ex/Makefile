#
# Makefile for OpenMP demo programs
#
# "make program" to make one program
# "make" or "make all" to make all executables
# "make clean" to remove executables
#

OMPC        = gcc
CFLAGS      = -O3 -g -D_GLIBCXX_PARALLEL -static-libgcc -L. -fopenmp   ### -Wall -pedantic

OMPCC       = g++
CCFLAGS     = -O3 -g -D_GLIBCXX_PARALLEL -static-libgcc -L. -fopenmp   ### -Wall -pedantic 

LFLAGS      = -lm 

ALL =   omp_hello \
	omp_num_threads \
	omp_shared  \
	omp_reduction  \
	omp_sections  \
	omp_critical \
	omp_for \
	omp_get_env_info \
	omp_threadprivate

#	pi_rect  \
#	pi_trap

#	omp_pi  \
#	omp_matrixmult \
#	omp_lastprivate  \

.PHONY:  all
all:  $(ALL)

%: %.c 
	$(OMPC) $(CFLAGS) $< -o $@ $(LFLAGS)

.PHONY:  clean
clean:
	-rm $(ALL)

