LEVEL=../
include $(LEVEL)/Makefile.common

clean:
	-sudo killall httpd
	-killall httpd-client && 0

eval: prepare clean
	sudo $(EXE_PREFIX) ./httpd &
	sleep 3
	./httpd-client -n 1 http://127.0.0.1/index.html
	sleep 3
	./httpd-client -n 1 http://127.0.0.1/index.html &
	./httpd-client -n 1 http://127.0.0.1/index.html &

