* These scripts have been tested on Ubuntu 12.04 x86_64/i686

==== Installation ====
1.1 Setup local gcc-4.2
    (Before doing this, please create a symbolic link first
    if your operating system is Ubuntu 64 bits machine.
    Make sure /usr/lib64 points to /usr/lib/x86_64-linux-gnu)
    ($ sudo ln -s /usr/lib/x86_64-linux-gnu /usr/lib64 )
    $ ./setup-gcc-4.2

1.2 Get required Parsec Benchmark Suite
    $ ./setup-parsec

