interpose.so: hook.cpp
	gcc -fPIC -rdynamic -g $^ -I/usr/include/ -shared -Wl,-soname,interpose.so -o interpose.so -ldl

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

all: interpose.so

clean: 
	rm *.o
	rm *.so
