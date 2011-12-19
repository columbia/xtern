LEVEL := ../..

LIBRARYNAME := path-slicer
BUILD_ARCHIVE := 1
LOADABLE_MODULE := 1

EXCLUDED_SOURCES := template.c
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(XTERN_ROOT)/include/ -I$(SLICER_ROOT)/include/ -I$(LLVM_ROOT) \
	-I$(SLICER_ROOT)/lib/stp/install/include/
