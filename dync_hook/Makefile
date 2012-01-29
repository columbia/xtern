all: xternruntime interpose.so 

INC_DIRS=-I$(XTERN_ROOT)/include/
STD_LIBS=-lsupc++ -lpthread -lstdc++
FLAG=`date +%F\#%T`
#TERN_LIBS=-L$(XTERN_ROOT)/obj/Debug/lib/  -lanalysis -linstr -lruntime
TERN_LIBS=-L$(XTERN_ROOT)/obj/Debug/lib/ -lruntime -lcommon 
#TERN_LIBS=
#TERN_LIBS=$(XTERN_ROOT)/obj/Debug/lib/libruntime.a
CFLAGS= -g $(INC_DIRS)
CXXFLAGS= -g $(INC_DIRS)

xternruntime: 
	cd $(XTERN_ROOT)/obj && make ENABLE_OPTIMIZED=0

interpose.so: template.cpp spec_hooks.cpp hook.cpp
	gcc -fPIC -rdynamic hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose.so interpose.o -o interpose.so -ldl $(TERN_LIBS) $(STD_LIBS)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

code_gen: code_gen.cpp
	g++ $^ $(CXXFLAGS) -o code_gen

template.cpp: code_gen func_template.cpp void_func_template.cpp hook_func.def
	./code_gen < hook_func.def

test: tests/server.cpp tests/client.cpp tests/all.cpp
	g++ tests/server.cpp -o server
	g++ tests/client.cpp -o client
	g++ tests/all.cpp -o all -pthread

runtests: test
#	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./server 10144 &
#	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./client 127.0.0.1 10144 &
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./all 10667

test_aget:
	rm -rf out
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./aget -f -n 8 http://www.cs.columbia.edu/~huayang/files/llg++

fft:
	rm -rf out
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./FFT -m6 -p8

eval:
	cd $(XTERN_ROOT)/apps/ && make -B EXE_PREFIX="env LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so"

clean: 
	rm -rf *.o
	rm -rf *.so
