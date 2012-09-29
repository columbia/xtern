all:: install

install::
	gcc -rdynamic -Wall -shared -fpic -o dyn-intercept-calls.so dyn-intercept-calls.c -lpthread -lrt -ldl
