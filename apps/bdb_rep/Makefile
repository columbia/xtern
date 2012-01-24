LEVEL=../
include $(LEVEL)/Makefile.common

MY_PREFIX= env LD_LIBRARY_PATH=$(APPS_DIR)/bdb/install/lib/ 

clean:
	-killall rep_mgr
	-killall rep_mgr_master
	rm -rf data1
	rm -rf data2
	rm -rf data3

eval: prepare clean 
#	export LD_LIBRARY_PATH=$(APPS_DIR)/bdb/install/lib/
#	echo $$LD_LIBRARY_PATH
	mkdir data1
	mkdir data2
	mkdir data3
	$(EXE_PREFIX) $(MY_PREFIX) ./rep_mgr_master -M -h data1 -l 127.0.0.1:10001 -r 127.0.0.1:10002 -r 127.0.0.1:10003 2> rep1.log &
	$(EXE_PREFIX) $(MY_PREFIX) ./rep_mgr -C -h data2 -l 127.0.0.1:10002 -r 127.0.0.1:10001 -r 127.0.0.1:10003 2> rep2.log &
	$(EXE_PREFIX) $(MY_PREFIX) ./rep_mgr -C -h data3 -l 127.0.0.1:10003 -r 127.0.0.1:10001 -r 127.0.0.1:10002 2> rep3.log &
	sleep 3
	pkill rep_mgr_master
	sleep 3
	pkill rep_mgr
