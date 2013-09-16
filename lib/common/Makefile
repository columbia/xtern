LEVEL := ../..

BUILT_SOURCES := options.cpp

LIBRARYNAME := common
BUILD_ARCHIVE := 1

include $(LEVEL)/Makefile.common

options.cpp: $(SRC_DIR)/gen-opt.pl $(SRC_ROOT)/default.options
	$(SRC_DIR)/gen-opt.pl $(SRC_ROOT)/default.options \
	$(OBJ_ROOT)/include/tern $(OBJ_DIR)
