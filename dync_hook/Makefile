all: interpose.so libxtern-annot.so usermonitor interpose_mc.so

INC_DIRS=-I$(TERN_ROOT)/include/ -I$(XTERN_ROOT)/include/ -I$(XTERN_ROOT)/obj/include
STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt
TERN_LIBS=-L$(XTERN_ROOT)/install/lib/ -lruntime -lcommon
CFLAGS= -g $(INC_DIRS)
CXXFLAGS= -g $(INC_DIRS)

interpose.so: template.cpp spec_hooks.cpp hook.cpp
ifeq ($(XTERN_PLUS_DBUG),0)
	gcc -fPIC -rdynamic hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose.so interpose.o -o interpose.so -ldl $(TERN_LIBS) $(STD_LIBS)
endif

interpose_mc.so: template.cpp spec_hooks.cpp hook.cpp
ifeq ($(XTERN_PLUS_DBUG),1)
	gcc -fPIC -rdynamic hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose_mc.so interpose.o -o interpose_mc.so -ldl $(TERN_LIBS) $(STD_LIBS) \
	-Wl,--rpath,$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -L$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -ldbug-stubs
endif

libxtern-annot.so: xtern-annot.cpp
	gcc -fPIC -rdynamic xtern-annot.cpp $(CFLAGS) -c -o xtern-annot.o
	gcc $(CFLAGS) -shared -Wl,-soname,libxtern-annot.so xtern-annot.o -o libxtern-annot.so -ldl $(TERN_LIBS) $(STD_LIBS)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

code_gen: code_gen.cpp
	g++ $^ $(CXXFLAGS) -o code_gen -ldl

template.cpp: code_gen func_template.cpp void_func_template.cpp hook_func.def
	./code_gen < hook_func.def

clean: 
	rm -rf *.o
	rm -rf *.so
	rm -rf code_gen template.cpp

