#killall rep_mgr
LD_PRELOAD=/home/huayang/research/xtern/DL/interpose.so ./rep_mgr -M -h data1 -l 127.0.0.1:10001 -r 127.0.0.1:10002 2> rep1.log 
#LD_PRELOAD=/home/huayang/research/xtern/DL/interpose.so ./rep_mgr -C -h data2 -l 127.0.0.1:10002 -r 127.0.0.1:10001 2> rep2.log &
