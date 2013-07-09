LEVEL := ../..

BUILT_SOURCES := options.cpp

LIBRARYNAME := common
BUILD_ARCHIVE := 1

# bc module
# MODULE_NAME := $(LIBRARYNAME)

include $(LEVEL)/Makefile.common

CFLAGS += -g -fPIC -rdynamic
CXXFLAGS += -g -fPIC -rdynamic 
LDFLAGS += -g -fPIC -rdynamic

options.cpp: $(PROJ_SRC_DIR)/gen-opt.pl $(PROJ_SRC_ROOT)/default.options
	$(PROJ_SRC_DIR)/gen-opt.pl $(PROJ_SRC_ROOT)/default.options \
	$(PROJ_OBJ_ROOT)/include/tern $(PROJ_OBJ_DIR)
