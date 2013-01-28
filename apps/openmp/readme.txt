0. Install some other stuff.
$ sudo apt-get install libgmp-dev libmpfr-dev libmpc-dev
$ sudo apt-get install gcc-4.5 g++-4.5 g++-4.5-multilib
And link your gcc-4.5 and g++-4.5 to be gcc and g++.

1. Link directory or file.
On 64 bits machine:
Nothing needs to be done.

On 32 bits machine:
$ sudo ln -s /usr/include/i386-linux-gnu/gnu/stubs-32.h /usr/include/gnu/stubs-32.h

2. Build.
$ cd $XTERN_ROOT/apps/openmp/
$ ./mk
Then you will see these files in local directory:
libgomp.a
libstdc++.a
ex-for
rand-para

3. Run with xtern.
LD_PRELOAD=$XTERN_ROOT/dync_hook/interpose.so ./ex-for

