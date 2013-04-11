LEVEL := ../..

BUILT_SOURCES = template
SOURCES = hook.cpp

LIBRARYNAME := interpose
LOADABLE_MODULE := 1
USEDLIBS = runtime.a common.a

CXXFLAGS += -Wl,-soname,interpose.so
LDFLAGS += -Wl,-soname,interpose.so

# bc module
# MODULE_NAME := $(LIBRARYNAME)

include $(LEVEL)/Makefile.common

template:
	cd $(PROJ_SRC_DIR) > /dev/null ; \
	g++ code_gen.cpp -o code_gen -ldl ; \
	$(PROJ_SRC_DIR)/code_gen < $(PROJ_SRC_DIR)/hook_func.def ; \
	cd - > /dev/null

