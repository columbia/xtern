LEVEL := ../..

BUILT_SOURCES := options.cpp

LIBRARYNAME := common
BUILD_ARCHIVE := 1

# bc module
# MODULE_NAME := $(LIBRARYNAME)

include $(LEVEL)/Makefile.common

options.cpp: $(SRC_DIR)/gen-opt.pl $(SRC_ROOT)/default.options
	$(SRC_DIR)/gen-opt.pl $(SRC_ROOT)/default.options \
	$(OBJ_ROOT)/include/tern $(OBJ_DIR)
