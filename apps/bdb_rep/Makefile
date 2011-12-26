LEVEL=../
include $(LEVEL)/Makefile.common

EXE_PREFIX+= LD_LIBRARY_PATH=$(APPS_DIR)/bdb/install/lib/ 

clean:
	-killall rep_mgr
	rm -rf data1
	rm -rf data2
	rm -rf data3

eval: prepare clean 
#	export LD_LIBRARY_PATH=$(APPS_DIR)/bdb/install/lib/
#	echo $$LD_LIBRARY_PATH
	mkdir data1
	mkdir data2
	mkdir data3
	$(EXE_PREFIX) ./rep_mgr -M -h data1 -l 127.0.0.1:10001 -r 127.0.0.1:10002 -r 127.0.0.1:10003 &
	$(EXE_PREFIX) ./rep_mgr -C -h data2 -l 127.0.0.1:10002 -r 127.0.0.1:10001 -r 127.0.0.1:10003 &
	$(EXE_PREFIX) ./rep_mgr -C -h data3 -l 127.0.0.1:10003 -r 127.0.0.1:10001 -r 127.0.0.1:10002 &

