Installing xtern
================

0. Add $XTERN_ROOT (the absolute path of "xtern") into environment variables
in your ~/.bashrc. Run "echo $XTERN_ROOT" to make sure it is correct.

1. Go to directory $XTERN_ROOT, and run:
> cd $XTERN_ROOT
> ./llvm/build-llvm.sh --optimized
You may need to install bison and flex on your machine because llvm needs them.
The "--optimized" flag above is optional, if you are getting performance results,
then you need this flag; if you are developing and need debug symbols,
then you don't need this flag. If you have specified this "--optimized" flag,
you need to specify "ENABLE_OPTIMIZED=1" in the following steps, otherwise 
specify "ENABLE_OPTIMIZED=0".

2. Create $XTERN_ROOT/obj, go to obj.

3. Do config as following. that's because xtern uses LLVM makefile.common:
> ./../configure --with-llvmsrc=$XTERN_ROOT/llvm/llvm-2.7/ \
  --with-llvmobj=$XTERN_ROOT/llvm/llvm-obj/ \
  --with-llvmgccdir=$XTERN_ROOT/llvm/install/bin/ \
  --prefix=$XTERN_ROOT/install

4. Make. Every time after you 'git pull' xtern, you should go to this directory and make it.
> cd $XTERN_ROOT/dync_hook
> make clean
> make ENABLE_OPTIMIZED=0/1

5. Append $XTERN_ROOT/dync_hook to your LD_LIBRARY_PATH in your ~/.bashrc.
Run "echo $LD_LIBRARY_PATH" to make sure it is correct.
> export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$XTERN_ROOT/dync_hook

6. Go to $XTERN_ROOT/eva/rand-intercept and run 'make'.

7. Test, if it all passes, then everything has been installed correctly.
> cd $XTERN_ROOT/obj
> make ENABLE_OPTIMIZED=0/1 test check






Testsuite (xtern/test)
======================

Our testsuite uses LLVM's test system, which is subsequently based on
dejagnu.

How to create a directory of tests
  - create a directory somewhere under xtern/test
  - cp dg.exp from another directory to this directory
  - note that dg.exp expects testcases with .ll, .c, or .cpp extensions;
    if your testcases use other extensions, please edit dg.exp accordingly

How to write a test
  - create a .ll, .c, or .cpp file
  - add the commands  you want to run as comments in the form of (for example)
    // RUN: command
    these RUN: lines will be automatically picked up by RunLLVMTests in
    test/lib/llvm.exp
  - when writing the commands, you can use the variables predefined in
    test/Makefile
  - %s represents the test file, and %t1, %t2, ... represent temporary files.

How to write check output of a test run against expected output?
  - can use the 'diff' command
  - can also use LLVM's FileCheck (http://llvm.org/cmds/FileCheck.html);
    see below

How to run tests?
  - run entire test suite
    # suppose we're at the root of the build directory
    make check
  - run one testcase only
    # suppose we're at the root of the build directory
    make -C test TESTONE=<path-to-test> check-one

    # for example, to run the test xtern/test/instr/instr-call-test.cpp, we use
    make -C test TESTONE=test/instr/instr-call-test.cpp check-one

  - output (e.g., stdout and stderr) of tests are at
    <buiid-dir>/test/<subdir>/Output

How to use FileCheck?
  - simple usage.  add the following line

     CHECK: text

    to a file containing the expected output, then run

       FileCheck <expected output>

    which checks that the stdin has a line that contains
    'text', e.g.,

       blah  text   blah

    note that "CHECK: text" matchines a line as long as the line
    contains the string text.

    the stdin can be output from other commands.  another trick is to
    include the expected output in the source file itself.  For instance, we can add

    // CHECK: text

    to a .cpp file containing the code of the testcase, then use check the
    output of the testcase against the .cpp file itself, and FileCheck
    will pick up only the CHECK: lines

  - multiple CHECK:

      CHECK: text1
      CHECK: text2
      CHECK: text3

    which checks that the stdin contains three (not necessarily
    consecutive) lines that match the three CHECKs

  - CHECK: can have pattern, and can do matching as well


Unit-testsuite (xtern/unittests)
================================

Our unit-test framework uses LLVM's, which is subsequently based on GoogleTest

How to create unit tests
  - check out the examples in xtern/unittests/runtime

How to run unittest
  # go to <build dir>, and run
  make -C unittests
  # then run the test program, for example
  ./unittests/runtime/Debug/recorderTests

