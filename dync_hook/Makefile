all: xternruntime interpose.so libxtern-annot.so usermonitor

INC_DIRS=-I$(TERN_ROOT)/include/ -I$(XTERN_ROOT)/include/ -I$(XTERN_ROOT)/obj/include
STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt
FLAG=`date +%F\#%T`
TERN_LIBS=-L$(XTERN_ROOT)/install/lib/ -lruntime -lcommon
CFLAGS= -g $(INC_DIRS)
CXXFLAGS= -g $(INC_DIRS)
PREFIX=LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so
#PREFIX=

usermonitor: usermonitor.cpp
	g++ usermonitor.cpp -o usermonitor

xternruntime:
	cd $(XTERN_ROOT)/obj && \
	make ENABLE_OPTIMIZED=$(ENABLE_OPTIMIZED) XTERN_PLUS_DBUG=$(XTERN_PLUS_DBUG) && \
	make ENABLE_OPTIMIZED=$(ENABLE_OPTIMIZED) XTERN_PLUS_DBUG=$(XTERN_PLUS_DBUG) install

interpose.so: template.cpp spec_hooks.cpp hook.cpp
	gcc -fPIC -rdynamic hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose.so interpose.o -o interpose.so -ldl $(TERN_LIBS) $(STD_LIBS)

libxtern-annot.so: xtern-annot.cpp
	gcc -fPIC -rdynamic xtern-annot.cpp $(CFLAGS) -c -o xtern-annot.o
	gcc $(CFLAGS) -shared -Wl,-soname,libxtern-annot.so xtern-annot.o -o libxtern-annot.so -ldl $(TERN_LIBS) $(STD_LIBS)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

code_gen: code_gen.cpp
	g++ $^ $(CXXFLAGS) -o code_gen -ldl

template.cpp: code_gen func_template.cpp void_func_template.cpp hook_func.def
	./code_gen < hook_func.def

compile_sc: tests/server.cpp tests/client.cpp tests/all.cpp
	g++ tests/server.cpp -o server
	g++ tests/client.cpp -o client
	g++ tests/all.cpp -o all -pthread

runtests: test_sc
#	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./server 10144 &
#	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./client 127.0.0.1 10144 &

test_sc: compile_sc
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./all 10667

test_aget:
	rm -rf out
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./aget -f -n 4 http://www.cs.columbia.edu/~huayang/files/demeter.pdf

clean_yfs:
	-pkill rand_tester 
	-pkill yfs_client
	-pkill lock_server
	-pkill extent_server
	-sudo umount yfs1
	-sudo umount yfs2
	-sudo umount yfs3
	-rm yfs1 -rf
	-rm yfs2 -rf
	-rm yfs3 -rf
	-rm *.log -rf

test_yfs: clean_yfs run_yfs
	-pkill rand_tester 
	-pkill yfs_client
	-pkill lock_server
	-pkill extent_server
	-sudo umount yfs1
	-sudo umount yfs2
	-sudo umount yfs3
	grep yfs_client tmp

AAAAA=$(shell date +%N > PORT.log)
RAND_NUM=$(shell cat PORT.log)
PORT_BASE=$(shell expr $(RAND_NUM) % 4000 + 20000)
LOCK_PORT=$(PORT_BASE)
EXTENT_PORT=$(shell expr $(PORT_BASE) + 1000)
prepare_yfs:
	date +%N > PORT.log
run_yfs: prepare_yfs
	rm -rf out
	$(PREFIX) ./lock_server $(PORT_BASE) $(PORT_BASE) > lock_server1.log 2>&1 &
	#./lock_server $(PORT_BASE) $(PORT_BASE) > lock_server1.log 2>&1 &
	sleep 5
	$(PREFIX) ./lock_server $(PORT_BASE) $(shell expr $(PORT_BASE) + 10) > lock_server2.log 2>&1 &
	#./lock_server $(PORT_BASE) $(shell expr $(PORT_BASE) + 10) > lock_server2.log 2>&1 &
	sleep 5
	$(PREFIX) ./lock_server $(PORT_BASE) $(shell expr $(PORT_BASE) + 20) > lock_server3.log 2>&1 &
	#./lock_server $(PORT_BASE) $(shell expr $(PORT_BASE) + 20) > lock_server3.log 2>&1 &
	sleep 5
	$(PREFIX) ./extent_server $(EXTENT_PORT) > extent_server.log 2>&1  &
	#./extent_server $(EXTENT_PORT) > extent_server.log 2>&1  &
	sleep 5
	mkdir -p yfs1
	$(PREFIX) ./yfs_client yfs1 $(EXTENT_PORT) $(LOCK_PORT) > yfs_client1.log 2>&1  &
	#./yfs_client yfs1 $(EXTENT_PORT) $(LOCK_PORT) > yfs_client1.log 2>&1  &
	sleep 5
	mount > tmp1
	mkdir -p yfs2
	$(PREFIX) ./yfs_client yfs2 $(EXTENT_PORT) $(LOCK_PORT) > yfs_client2.log 2>&1  &
	#./yfs_client yfs2 $(EXTENT_PORT) $(LOCK_PORT) > yfs_client2.log 2>&1  &
	sleep 5
	./rand_tester yfs1 yfs2 & 
	#./rand_tester yfs1  &
	sleep 10
	mount > tmp 

fft:
	rm -rf out
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./FFT -m24 -p4

eval:
	cd $(XTERN_ROOT)/apps/ && make -B EXE_PREFIX="env LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so"

clean: 
	rm -rf *.o
	rm -rf *.so
	- rm code_gen
