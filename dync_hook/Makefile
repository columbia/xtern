#
# Relative path to the top of the source tree.
#
LEVEL := ..
include $(LEVEL)/Makefile.common

INC_DIRS=-I$(TERN_ROOT)/include/ -I$(XTERN_ROOT)/include/ -I$(XTERN_ROOT)/obj/include -I.
STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt
TERN_LIBS=-L$(PROJ_OBJ_ROOT)/$(BuildMode)/lib/ -lruntime -lcommon
CFLAGS= -g $(INC_DIRS)
CXXFLAGS= -g $(INC_DIRS)
SRC_DIR=$(XTERN_ROOT)/dync_hook
ifndef $(XTERN_PLUS_DBUG)
	XTERN_PLUS_DBUG=0
endif

all:: interpose.so libxtern-annot.so interpose_mc.so

interpose.so: template.cpp spec_hooks.cpp hook.cpp
ifneq ($(XTERN_PLUS_DBUG),1)
	gcc -fPIC -rdynamic $(SRC_DIR)/hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose.so interpose.o -o interpose.so -ldl $(TERN_LIBS) $(STD_LIBS)
endif

interpose_mc.so: template.cpp spec_hooks.cpp hook.cpp
ifeq ($(XTERN_PLUS_DBUG),1)
	gcc -fPIC -rdynamic $(SRC_DIR)/hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose_mc.so interpose.o -o interpose_mc.so -ldl $(TERN_LIBS) $(STD_LIBS) \
	-Wl,--rpath,$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -L$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -ldbug-stubs
endif

libxtern-annot.so: xtern-annot.cpp
	gcc -fPIC -rdynamic $(SRC_DIR)/xtern-annot.cpp $(CFLAGS) -c -o xtern-annot.o
	gcc $(CFLAGS) -shared -Wl,-soname,libxtern-annot.so xtern-annot.o -o libxtern-annot.so -ldl $(TERN_LIBS) $(STD_LIBS)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

code_gen: code_gen.cpp
	g++ $^ $(CXXFLAGS) -o code_gen -ldl

template.cpp: code_gen func_template.cpp void_func_template.cpp hook_func.def
	./code_gen $(SRC_DIR)/func_template.cpp $(SRC_DIR)/void_func_template.cpp < $(SRC_DIR)/hook_func.def

clean::
	rm -rf *.o
	rm -rf *.so
	rm -rf code_gen template.cpp
	rm -rf $(SRC_DIR)/*.o
	rm -rf $(SRC_DIR)/*.so
	rm -rf $(SRC_DIR)/code_gen $(SRC_DIR)/template.cpp

install::
	cp *.so $(SRC_DIR)

