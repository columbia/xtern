all: interpose.so template.cpp

interpose.so: hook.cpp
	gcc -fPIC -rdynamic -g $^ -I/usr/include/ -shared -Wl,-soname,interpose.so -o interpose.so -ldl

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

code_gen: code_gen.cpp
	g++ $^ -o code_gen

template.cpp: code_gen func_template.cpp void_func_template.cpp hook_func.def
	./code_gen < hook_func.def > template.cpp

tests: tests/server.cpp tests/client.cpp
	g++ tests/server.cpp -o server
	g++ tests/client.cpp -o client

runtests: tests
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./server 10124 &
	LD_PRELOAD=$(XTERN_ROOT)/dync_hook/interpose.so ./client 127.0.0.1 10124 &

clean: 
	rm -rf *.o
	rm -rf *.so
