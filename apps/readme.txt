=== aget

./aget [options] url 
e.g. 8 threads to download demeter.pdf
./aget -f -n 8 http://www.cs.columbia.edu/~huayang/files/demeter.pdf

=== bdb

First add bdb/install/lib/ into LD_LIBRARY_PATH:
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$APPS_DIR/bdb/install/lib/

usage: ex_rep_mgr [-CM]-h home -l|-L host:port [-r host:port][-R host:port][-a all|quorum][-b][-p priorit

mkdir data1
mkdir data2
mkdir data3
./rep_mgr -M -h data1 -l 127.0.0.1:10001 -r 127.0.0.1:10002 -r 127.0.0.1:10003
./rep_mgr -C -h data2 -l 127.0.0.1:10002 -r 127.0.0.1:10001 -r 127.0.0.1:10003
./rep_mgr -C -h data3 -l 127.0.0.1:10003 -r 127.0.0.1:10001 -r 127.0.0.1:10002


== apache

kill httpd processes at first
html files locate in $APPS_DIR/apache/apache-install/htdocs

sudo ./httpd
./httpd-client -n 1 http://127.0.0.1/index.html
