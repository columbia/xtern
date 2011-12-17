How to build xtern with dynamic hook module.
1. create $XTERN_ROOT/obj, go to obj.
2. do config as following. that's because xtern uses LLVM makefile.common
> ./../configure --with-llvmsrc=$LLVM_ROOT/llvm-2.7/ --with-llvmobj=$LLVM_ROOT/llvm-obj/ --with-llvmgccdir=$LLVM_ROOT/install/bin/
3. edit obj/lib/Makefile, and remove "instr" and "analysis"
4. edit obj/lib/runtime/Makefile, and remove the line of MODULE_NAME
5. do "make" in obj. sometimes "make ENABLE_OPTIMIZED=0/1"
