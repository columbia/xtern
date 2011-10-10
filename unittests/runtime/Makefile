LEVEL := ../../

TESTNAME := recorder
USEDLIBS := analysis.a runtime.a common.a
LINK_COMPONENTS := core support scalaropts

include $(LEVEL)/Makefile.config
include $(LLVM_SRC_ROOT)/unittests/Makefile.unittest

# FIXME: better way to link in our libraries in llvm/install/lib
LIBS += $(LLVM_ROOT)/install/lib/libid-manager.a \
	$(LLVM_ROOT)/install/lib/libLLVMSupport.a
