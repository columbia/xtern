1 Checkout git repo.    
	git clone git@repair.cs.columbia.edu:xtern
	git clone git@repair.cs.columbia.edu:llvm


2 Install llvm.
	Common RCS workflow.


3 Add environment variables.
	Here is an example:
	export XTERN_ROOT=/home/heming/rcs/xtern
	export LLVM_ROOT=/home/heming/rcs/llvm


4 Installation.
	cd $XTERN_ROOT
	./configure --with-llvmsrc=$LLVM_ROOT/llvm-2.7 \
		--with-llvmobj=$LLVM_ROOT/llvm-obj --prefix=$LLVM_ROOT/install
	make ENABLE_OPTIMIZED=X (X is 0 or 1)
	make ENABLE_OPTIMIZED=X install (X is 0 or 1)


