Dynamic Hooks
=============

How to build xtern with dynamic hook module.
0. add $XTERN_ROOT into environment variables. make sure LLVM is correctly setup in debug mode. copy all the files from $LLVM_ROOT/llvm-obj/Debug/bin/ to $LLVM_ROOT/install/bin/
1. create $XTERN_ROOT/obj, go to obj.
2. do config as following. that's because xtern uses LLVM makefile.common
> ./../configure --with-llvmsrc=$LLVM_ROOT/llvm-2.7/ --with-llvmobj=$LLVM_ROOT/llvm-obj/ --with-llvmgccdir=$LLVM_ROOT/install/bin/
2.5 run "make ENABLE_OPTIMIZED=0" in obj directory. it fails this time. 
3. edit obj/lib/Makefile, and remove "instr" and "analysis"
4. edit obj/lib/runtime/Makefile, and remove the line of MODULE_NAME
5. do "make ENABLE_OPTIMIZED=0"
6. goto $XTERN_ROOT/dync_hook and run 'make'.

You can test the build by running 'make test_sc' in $XTERN_ROOT/dync_hook

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

