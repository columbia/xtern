LEVEL := ../..

LIBRARYNAME := commoninstr
BUILD_ARCHIVE := 1

EXCLUDED_SOURCES := template.c
include $(LEVEL)/Makefile.common

all:: $(LibDir)/template.bc

$(LibDir)/template.bc: $(PROJ_SRC_DIR)/template.c
	$(LLVMGCC) -O0 -I$(PROJ_SRC_ROOT)/common/include -emit-llvm -o $@ -c $<