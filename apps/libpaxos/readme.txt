Steps to build and run libpaxos.
1. Unzip bdb src (because libpaxos uses bdb).
> cd $APPS_DIR/bdb
> cp $APPS_DIR/sys/db-5.2.36.tar.gz .

2. Setup libpaxos.
> cd $APPS_DIR/libpaxos
> ./setup 104
> ./mk 104

3. Run libpaxos (must reset the net.core.rmem_max, because the libpaxos
expr uses a very large system socket buffer size).
> cd $APPS_DIR/libpaxos/scripts/
> sudo sysctl -w net.core.rmem_max=8388608
> sudo sysctl -w net.core.wmem_max=8388608
> ./run.sh

The run.sh may take a few minutes to run.
