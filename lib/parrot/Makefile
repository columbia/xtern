LEVEL := ../..

BUILT_SOURCES = template
SOURCES = hook.cpp

LIBRARYNAME := interpose
LOADABLE_MODULE := 1
USEDLIBS = runtime.a common.a

include $(LEVEL)/Makefile.common

# use optimization level higher than 2 will cause backtrace fail
OPTIMIZE_OPTION = -O1

CFLAGS += -g -fPIC -rdynamic
CXXFLAGS += -g -fPIC -rdynamic
LDFLAGS += -g -fPIC -rdynamic

template: code_gen
	./code_gen $(PROJ_SRC_DIR)/func_template.cpp $(PROJ_SRC_DIR)/void_func_template.cpp < $(PROJ_SRC_DIR)/hook_func.def

code_gen:
	g++ $(PROJ_SRC_DIR)/code_gen.cpp -g -Os -o code_gen -ldl
